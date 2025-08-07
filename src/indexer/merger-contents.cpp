# include "merger-contents.hpp"
# include "contents-index-merger.hpp"
# include "../../indexer/static-contents.hpp"
# include "override-entities.hpp"
# include "index-layers.hpp"
# include "patch-table.hpp"
# include <mtc/recursive_shared_mutex.hpp>
# include <condition_variable>
# include <shared_mutex>
# include <stdexcept>
# include <thread>

namespace DelphiX {
namespace indexer {
namespace fusion  {

  class ContentsIndex final: protected IndexLayers,	public IContentsIndex
  {
    using ISerialized = IStorage::ISerialized;

    implement_lifetime_control

  public:
    ContentsIndex(
      const std::vector<mtc::api<IContentsIndex>>&  ixset,
      ContentsMerger&&                              merge,
      Notify::Func                                  event );
   ~ContentsIndex();

  public:
    auto  StartMerger() -> mtc::api<IContentsIndex>;

  public:     // overridables
    auto  GetEntity( EntityId ) const -> mtc::api<const IEntity> override;
    auto  GetEntity( uint32_t ) const -> mtc::api<const IEntity> override;

    bool  DelEntity( EntityId ) override;
    auto  SetEntity( EntityId, mtc::api<const IContents>,
      const StrView& ) -> mtc::api<const IEntity> override;
    auto  SetExtras( EntityId,
      const StrView& ) -> mtc::api<const IEntity> override;

    auto  GetMaxIndex() const -> uint32_t override;
    auto  GetKeyBlock( const StrView& ) const -> mtc::api<IEntities> override;
    auto  GetKeyStats( const StrView& ) const -> BlockInfo override;

    auto  GetEntityIterator( EntityId ) -> mtc::api<IEntityIterator> override
      {  throw std::runtime_error( "not implemented @" __FILE__ ":" LINE_STRING );  }
    auto  GetEntityIterator( uint32_t ) -> mtc::api<IEntityIterator> override
      {  throw std::runtime_error( "not implemented @" __FILE__ ":" LINE_STRING );  }

    auto  GetRecordIterator( const StrView& ) -> mtc::api<IRecordIterator> override
      {  throw std::runtime_error( "not implemented @" __FILE__ ":" LINE_STRING );  }

    auto  Commit() -> mtc::api<IStorage::ISerialized> override;
    auto  Reduce() -> mtc::api<IContentsIndex> override;
    void  Remove() override;
    void  Stash( EntityId ) override  {}

  protected:
    void  MergerThreadFunc();

  protected:
    mutable std::shared_mutex     swLock;   // switch mutex
    mtc::api<IContentsIndex>      output;
    mtc::api<ISerialized>         serial;
    ContentsMerger                merger;
    Notify::Func                  notify;

    std::mutex                    s_lock;   // notification
    std::condition_variable       s_wait;

    Bitmap<>                      banset;
    mutable PatchTable<>          hpatch;

    std::thread                   thread;
    std::exception_ptr            except;

  };

  // ContentsIndex implementation

  ContentsIndex::ContentsIndex(
    const std::vector<mtc::api<IContentsIndex>>&  ixset,
    ContentsMerger&&                              merge,
    Notify::Func                                  event ): IndexLayers( ixset ),
      merger( std::move( merge ) ),
      notify( event ),
      banset( getMaxIndex() + 1 ),
      hpatch( UpperPrime( std::min( 1000U, getMaxIndex() ) ) ) {}

  ContentsIndex::~ContentsIndex()
  {
    if ( thread.joinable() )
      thread.join();
  }

  auto  ContentsIndex::StartMerger() -> mtc::api<IContentsIndex>
  {
    thread = std::thread( &ContentsIndex::MergerThreadFunc, this );
    return this;
  }

  void  ContentsIndex::MergerThreadFunc()
  {
    pthread_setname_np( pthread_self(), "merger::Thread" );

    for ( auto& next: layers )
      merger.Add( next.pIndex );

  // first merge index to the storage
  // then try open the new static index from the storage
    try
    {
      auto  target = merger();      // store to ISerialized
      auto  exlock = mtc::make_unique_lock( swLock );

    // serialize accumulated changes, dispose old index and open new static
      hpatch.Commit( serial = target );
      output = static_::Index().Create( serial = target );

      for ( auto& next: layers )
        next.pIndex->Remove();

    // notify merger finished
      s_wait.notify_all();

    // notify serialize finished
      if ( notify != nullptr )
        notify( this, Notify::Event::OK );
    }
    catch ( ... )
    {
      except = std::current_exception();

      if ( notify != nullptr )
        notify( this, Notify::Event::Failed );
    }
  }

  auto  ContentsIndex::GetEntity( EntityId id ) const -> mtc::api<const IEntity>
  {
    auto  shlock = mtc::make_shared_lock( swLock );

    if ( except != nullptr )
      std::rethrow_exception( except );

    if ( output == nullptr )
    {
      auto  entity = getEntity( id );
      auto  ppatch = mtc::api<const mtc::IByteBuffer>{};

      if ( entity == nullptr || (ppatch = hpatch.Search( { id.data(), id.size() } )) == nullptr )
        return entity;

      if ( ppatch->GetLen() == size_t(-1) )
        return nullptr;

      return Override::Entity( entity ).Extra( ppatch );
    }
    return output->GetEntity( id );
  }

  auto  ContentsIndex::GetEntity( uint32_t ix ) const -> mtc::api<const IEntity>
  {
    auto  shlock = mtc::make_shared_lock( swLock );

    if ( except != nullptr )
      std::rethrow_exception( except );

    if ( output == nullptr )
    {
      auto  entity = getEntity( ix );
      auto  ppatch = mtc::api<const mtc::IByteBuffer>{};

      if ( entity == nullptr || (ppatch = hpatch.Search( ix )) == nullptr )
        return entity;

      if ( ppatch->GetLen() == size_t(-1) )
        return nullptr;

      return Override::Entity( entity ).Extra( ppatch );
    }
    return output->GetEntity( ix );
  }

  bool  ContentsIndex::DelEntity( EntityId id )
  {
    auto  shlock = mtc::make_shared_lock( swLock );

    if ( except != nullptr )
      std::rethrow_exception( except );

    if ( output == nullptr )
    {
      auto  entity = getEntity( id );
      auto  ppatch = mtc::api<const mtc::IByteBuffer>{};

      if ( entity == nullptr )
        return false;

      if ( (ppatch = hpatch.Search( { id.data(), id.size() } )) == nullptr || ppatch->GetLen() != size_t(-1) )
      {
        hpatch.Delete( { id.data(), id.size() }, entity->GetIndex() );
        banset.Set( entity->GetIndex() );
        return true;
      }
      return false;
    }
    return output->DelEntity( id );
  }

  auto  ContentsIndex::SetEntity( EntityId, mtc::api<const IContents>, const StrView& ) -> mtc::api<const IEntity>
  {
    throw std::logic_error( "merger::SetEntity(...) must not be called" );
  }

  auto  ContentsIndex::SetExtras( EntityId id, const StrView& xtra ) -> mtc::api<const IEntity>
  {
    auto  shlock = mtc::make_shared_lock( swLock );

    if ( except != nullptr )
      std::rethrow_exception( except );

    if ( output == nullptr )
    {
      auto  entity = getEntity( id );
      auto  ppatch = mtc::api<const mtc::IByteBuffer>{};

      if ( entity == nullptr )
        return nullptr;

      if ( (ppatch = hpatch.Search( { id.data(), id.size() } )) != nullptr && ppatch->GetLen() == size_t(-1) )
        return nullptr;

      return hpatch.Update( { id.data(), id.size() }, entity->GetIndex(), xtra )->GetLen() != size_t(-1) ?
        GetEntity( id ) : nullptr;
    }
    return output->SetExtras( id, xtra );
  }

  auto  ContentsIndex::GetMaxIndex() const -> uint32_t
  {
    auto  shlock = mtc::make_shared_lock( swLock );

    if ( except != nullptr )
      std::rethrow_exception( except );

    return output != nullptr ? output->GetMaxIndex() : getMaxIndex();
  }

  auto  ContentsIndex::GetKeyBlock( const StrView& key ) const -> mtc::api<IEntities>
  {
    auto  shlock = mtc::make_shared_lock( swLock );
    auto  pblock = mtc::api<IEntities>();

    if ( except != nullptr )
      std::rethrow_exception( except );

    if ( output != nullptr )
      return output->GetKeyBlock( key );

    if ( (pblock = getKeyBlock( key )) == nullptr )
      return nullptr;

    return new Override::Entities( pblock, banset, this );
  }

  auto  ContentsIndex::GetKeyStats( const StrView& key ) const -> BlockInfo
  {
    auto  shlock = mtc::make_shared_lock( swLock );

    if ( except != nullptr )
      std::rethrow_exception( except );

    return output != nullptr ? output->GetKeyStats( key ) : getKeyStats( key );
  }

  auto  ContentsIndex::Commit() -> mtc::api<IStorage::ISerialized>
  {
    std::unique_lock<std::mutex>  exlock( s_lock );
      s_wait.wait( exlock, [&](){  return serial != nullptr;  } );
    return serial;
  }

  auto  ContentsIndex::Reduce() -> mtc::api<IContentsIndex>
  {
  // wait until the merger completes
    if ( thread.joinable() )
      thread.join();

    if ( except != nullptr )
      std::rethrow_exception( except );

    return output;
  }

  void  ContentsIndex::Remove()
  {
    throw std::runtime_error( "not implemented @" __FILE__ ":" LINE_STRING );
  }

  // Index implementation

  auto  Contents::Add( const mtc::api<IContentsIndex> i ) -> Contents&
  {
    indexVector.push_back( i );  return *this;
  }

  auto  Contents::Set( Notify::Func fn ) -> Contents&
  {
    notifyEvent = fn;  return *this;
  }

  auto  Contents::Set( std::function<bool()> fCanContinue ) -> Contents&
  {
    canContinue = fCanContinue;  return *this;
  }

  auto  Contents::Set( mtc::api<IStorage::IIndexStore> px ) -> Contents&
  {
    outputStore = px;  return *this;
  }

  auto  Contents::Set( const mtc::api<IContentsIndex>* pp, size_t cc ) -> Contents&
  {
    indexVector.clear();
    indexVector.insert( indexVector.end(), pp, pp + cc );  return *this;
  }

  auto  Contents::Set( const std::vector<mtc::api<IContentsIndex>>& rv ) -> Contents&
  {
    return Set( rv.data(), rv.size() );
  }

  auto  Contents::Set( const std::initializer_list<mtc::api<IContentsIndex>>& il ) -> Contents&
  {
    indexVector.clear();

    for ( auto& index: il )
      Add( index );

    return *this;
  }

  auto  Contents::Create() -> mtc::api<IContentsIndex>
  {
    return (new ContentsIndex( indexVector, std::move( ContentsMerger()
      .Set( indexVector )
      .Set( canContinue )
      .Set( outputStore ) ), notifyEvent ))->StartMerger();
  }

}}}
