# include "../../indexer/layered-contents.hpp"
# include "../../indexer/static-contents.hpp"
# include "../../indexer/dynamic-contents.hpp"
# include "../../exceptions.hpp"
# include "../../compat.hpp"
# include "commit-contents.hpp"
# include "merger-contents.hpp"
# include "object-holders.hpp"
# include "index-layers.hpp"
# include <mtc/recursive_shared_mutex.hpp>
# include <shared_mutex>
# include <cmath>

namespace DelphiX {
namespace indexer {
namespace layered {

  class ContentsIndex final: protected IndexLayers, public IContentsIndex
  {
    std::atomic_long  referenceCount = 0;

    long  Attach() override {  return ++referenceCount;  }
    long  Detach() override;

  public:
    ContentsIndex( const mtc::api<IContentsIndex>* indices, size_t count );
    ContentsIndex( const mtc::api<IStorage>&, const dynamic::Settings& );

    auto  StartMonitor( const std::chrono::seconds& mergeMonitorDelay ) -> ContentsIndex*;

  public:
    auto  GetEntity( EntityId ) const -> mtc::api<const IEntity> override;
    auto  GetEntity( uint32_t ) const -> mtc::api<const IEntity> override;

    bool  DelEntity( EntityId ) override;
    auto  SetEntity( EntityId, mtc::api<const IContents>,
      const std::string_view&, const std::string_view& ) -> mtc::api<const IEntity> override;
    auto  SetExtras( EntityId, const std::string_view& ) -> mtc::api<const IEntity> override;

    auto  GetMaxIndex() const -> uint32_t override;
    auto  GetKeyBlock( const std::string_view& ) const -> mtc::api<IEntities> override;
    auto  GetKeyStats( const std::string_view& ) const -> BlockInfo override;

    auto  ListEntities( EntityId ) -> mtc::api<IEntitiesList> override
      {  throw std::runtime_error( "not implemented @" __FILE__ ":" LINE_STRING );  }
    auto  ListEntities( uint32_t ) -> mtc::api<IEntitiesList> override
      {  throw std::runtime_error( "not implemented @" __FILE__ ":" LINE_STRING );  }
    auto  ListContents( const std::string_view& ) -> mtc::api<IContentsList> override;

    auto  Commit() -> mtc::api<IStorage::ISerialized> override;
    auto  Reduce() -> mtc::api<IContentsIndex> override {  return this;  }
    void  Remove() override;

    void  Stash( EntityId ) override  {}

  protected:
    using LayersIt = decltype(layers)::iterator;
    using EventRec = std::pair<void*, Notify::Event>;

    void  MergeMonitor( const std::chrono::seconds& );
    auto  SelectLimits() -> std::pair<LayersIt, LayersIt>;
    auto  WaitGetEvent( const std::chrono::seconds& ) -> EventRec;

  protected:
    mtc::api<IStorage>          istore;
    dynamic::Settings           dynSet;
    bool                        rdOnly = false;

    volatile bool               canRun = true;    // the continue flag

    mutable std::shared_mutex   ixlock;

  // event manager - the events are processed after the index
  // asyncronous action is performed
    std::list<EventRec>         evQueue;
    std::mutex                  evMutex;
    std::condition_variable     evEvent;
    std::thread                 monitor;
    volatile bool               merging = false;
  };

  // ContentsIndex implementation

  ContentsIndex::ContentsIndex( const mtc::api<IContentsIndex>* indices, size_t count ):
    IndexLayers( indices, count )
  {
  }

  ContentsIndex::ContentsIndex( const mtc::api<IStorage>& storage, const dynamic::Settings& dynamicSets ):
    IndexLayers(), istore( storage ), dynSet( dynamicSets )
  {
    auto  sources = istore->ListIndices();
    auto  dynamic = istore->CreateStore();

  // check if has any sources
    if ( sources != nullptr )
      for ( auto serial = sources->Get(); serial != nullptr; serial = sources->Get() )
        addContents( static_::Index().Create( serial ) );

  // add dynamic index to the end if possible
    if ( dynamic != nullptr )
    {
      addContents( dynamic::Index()
        .Set( dynamic )
        .Set( dynSet ).Create() );
      layers.back().uUpper = uint32_t(-1);
      layers.back().dwSets = 1;
      rdOnly = false;
    } else rdOnly = true;
  }

  auto  ContentsIndex::StartMonitor( const std::chrono::seconds& mergeMonitorDelay ) -> ContentsIndex*
  {
    monitor = std::thread( &ContentsIndex::MergeMonitor, this, mergeMonitorDelay );
    return this;
  }

  long  ContentsIndex::Detach()
  {
    auto  rcount = --referenceCount;

    if ( rcount == 0 )
    {
      if ( monitor.joinable() )
      {
        canRun = false,
          evEvent.notify_one();
        monitor.join();
      }
      commitItems();
      delete this;
    }
    return rcount;
  }


  auto  ContentsIndex::GetEntity( EntityId id ) const -> mtc::api<const IEntity>
  {
    auto  shlock = mtc::make_shared_lock( ixlock );

    return getEntity( id );
  }

  auto  ContentsIndex::GetEntity( uint32_t id ) const -> mtc::api<const IEntity>
  {
    auto  shlock = mtc::make_shared_lock( ixlock );

    return getEntity( id );
  }

  bool  ContentsIndex::DelEntity( EntityId id )
  {
    auto  shlock = mtc::make_shared_lock( ixlock );

    return delEntity( id );
  }

  auto  ContentsIndex::SetEntity( EntityId id, mtc::api<const IContents> contents,
    const std::string_view& xtra, const std::string_view& beef ) -> mtc::api<const IEntity>
  {
    if ( layers.empty() )
      throw std::logic_error( "index flakes are not initialized" );

    for ( ; ; )
    {
      auto  shlock = mtc::make_shared_lock( ixlock );
      auto  exlock = mtc::make_unique_lock( ixlock, std::defer_lock );
      auto  pindex = layers.back().pIndex.ptr();    // the last index pointer, unchanged in one thread

    // try Set the entity to the last index in the chain
      try
      {
        return layers.back().Override( pindex->SetEntity( id, contents, xtra, beef ) );
      }

    // on dynamic index overflow, rotate the last index by creating new one in a new flakes,
    // and continue Setting attempts
      catch ( const index_overflow& /*xo*/ )
      {
        shlock.unlock();  exlock.lock();

      // received exclusive lock, check if index is already rotated by another
      // SetEntity call; if yes, try again to SetEntity, else rotate index
        if ( layers.back().pIndex.ptr() == pindex )
        {
        // rotate the index by creating the commiter for last (dynamic) index
        // and create the new dynamic index
          layers.back().uUpper = layers.back().uLower
            + pindex->GetMaxIndex() - 1;

          layers.back().pIndex = commit::Contents().Create( layers.back().pIndex, [this]( void* to, Notify::Event event )
            {
              mtc::interlocked( mtc::make_unique_lock( evMutex ), [&]()
                {  evQueue.emplace_back( to, event );  } );
              evEvent.notify_one();
            } );

          layers.emplace_back( layers.back().uUpper + 1, dynamic::Index()
            .Set( dynSet )
            .Set( istore->CreateStore() ).Create() );
          layers.back().uUpper = (uint32_t)-1;
          layers.back().dwSets = 1;
        }
      }
    }
  }

  auto  ContentsIndex::SetExtras( EntityId id, const std::string_view& extras ) -> mtc::api<const IEntity>
  {
    auto  shlock = mtc::make_shared_lock( ixlock );
    return setExtras( id, extras );
  }

  auto  ContentsIndex::Commit() -> mtc::api<IStorage::ISerialized>
  {
    auto  shlock = mtc::make_shared_lock( ixlock );

    return commitItems(), nullptr;
  }

  void  ContentsIndex::Remove()
  {
    throw std::runtime_error( "not implemented @" __FILE__ ":" LINE_STRING );
  }

  auto  ContentsIndex::GetMaxIndex() const -> uint32_t
  {
    auto  shlock = mtc::make_shared_lock( ixlock );
    return getMaxIndex();
  }

  auto  ContentsIndex::GetKeyBlock( const std::string_view& key ) const -> mtc::api<IEntities>
  {
    return mtc::interlocked( mtc::make_shared_lock( ixlock ), [&]()
      {  return getKeyBlock( key, this );  } );
  }

  auto  ContentsIndex::GetKeyStats( const std::string_view& key ) const -> BlockInfo
  {
    return mtc::interlocked( mtc::make_shared_lock( ixlock ), [&]()
      {  return getKeyStats( key );  } );
  }

  auto  ContentsIndex::ListContents( const std::string_view& key ) -> mtc::api<IContentsList>
  {
    return listContents( key, MakeObjectHolder( mtc::api( (const Iface*)this ),
      std::move( mtc::make_shared_lock( ixlock ) ) ) );
  }

  void  ContentsIndex::MergeMonitor( const std::chrono::seconds& startDelay )
  {
    pthread_setname_np( pthread_self(), "MergeMonitor" );

    for ( std::this_thread::sleep_for( startDelay ); canRun; )
    {
      auto  evNext = WaitGetEvent( std::chrono::seconds( 30 ) );

    // for event occured, search the element in the list of indices to Reduce()
    // and finish index modification
      if ( evNext.first != nullptr && canRun)
      {
        auto  exlock = mtc::make_unique_lock( ixlock );
        auto  pfound = std::find_if( layers.begin(), layers.end(), [&]( const IndexEntry& index )
          {  return index.pIndex.ptr() == evNext.first;  } );

      // if the index with key pointer found, check the type of event occured
        if ( pfound == layers.end() )
          throw std::logic_error( "strange event not attached to any index!" );

        switch ( evNext.second )
        {
        // On OK, replace the index in the entry to it's reduced version,
        // resort the indices in the size-decreasing order, and renumber
          case Notify::Event::OK:
          {
            uint32_t uLower = 1;

            pfound->pIndex = pfound->pIndex->Reduce();
            pfound->backup.clear();
            pfound->dwSets = 0;

            std::sort( layers.begin(), layers.end() - 1, []( const IndexEntry& a, const IndexEntry& b )
            {
              if ( a.dwSets != b.dwSets )
                return (a.dwSets != 0) > (b.dwSets != 0 );
              return a.pIndex->GetMaxIndex() > b.pIndex->GetMaxIndex();
            } );

            for ( auto& index: layers )
              uLower = (index.uUpper = (index.uLower = uLower) + index.pIndex->GetMaxIndex() - 1) + 1;

            layers.back().uUpper = uint32_t(-1);
            break;
          }

        // On Empty, simple remove the existing index because its processing
        // result is empty
          case Notify::Event::Empty:
            layers.erase( pfound );
            break;

        // On Cancel, rollback the event record to the previous subset
        // of entries saved in the entry processed
          case Notify::Event::Canceled:
          {
            auto  backup = std::move( pfound->backup );

            layers.insert( layers.erase( pfound ),
              backup.begin(), backup.end() );
            break;
          }

      // On Failed, commit index and shutdown service if possible
          default:
            break;
        }
      }

    // try select indices to be merged
      if ( canRun )
      {
        auto  shlock = mtc::make_shared_lock( ixlock );
        auto  exlock = mtc::make_unique_lock( ixlock, std::defer_lock );
        auto  limits = SelectLimits();

      // select the limits, check and select again the limits for merger
        if ( limits.first != limits.second )
        {
          shlock.unlock();  exlock.lock();

          if ( (limits = SelectLimits()).first != limits.second )
          {
            auto  xMaker = fusion::Contents()
              .Set( [this]( void* to, Notify::Event event )
                {
                  mtc::interlocked( mtc::make_unique_lock( evMutex ), [&]()
                    {  evQueue.emplace_back( to, event );  } );
                  merging = false;
                    evEvent.notify_one();
                } )
//              .Set( canContinue )
              .Set( istore->CreateStore() );

            for ( auto p = limits.first; p != limits.second; ++p )
            {
              xMaker.Add( p->pIndex );
              limits.first->backup.push_back( IndexEntry{ p->uLower, p->pIndex } );
            }

            limits.first->uUpper = limits.first->backup.back().uUpper;
            limits.first->pIndex = xMaker.Create();
            limits.first->dwSets = 1;

            layers.erase( limits.first + 1, limits.second );

            merging = true;
          }
        }
      }
    }
  }

 /*
  * Ищет самую длинную постедовательность самых маленьких индексов. Критерий -
  */
  auto  ContentsIndex::SelectLimits() -> std::pair<LayersIt, LayersIt>
  {
    auto  asizes = std::vector<size_t>();
    auto  select = std::make_pair( layers.end(), layers.end() );
    float srange;
    auto  Ranker = [&]( size_t from, size_t to ) -> float
    {
      auto  min_size = size_t(-1);
      auto  max_size = size_t(0);
      auto  med_size = size_t(0);

      for ( auto i = from; i != to; ++i )
      {
        min_size = std::min( min_size, asizes[i] );
        max_size = std::max( max_size, asizes[i] );
        med_size += asizes[i];
      }
      med_size /= (to - from);

      auto  s_factor = 1 / (1 + log(1 + (max_size - min_size) / 1000.0));
      auto  l_factor = 0.2 + sin(1.57 + atan((med_size - 1) / 10000.0)) * 0.8;
      auto  n_factor = 0.2 + sin(2 * atan((to - from - 2) / 6.0)) * 0.8;

      return float(s_factor * l_factor * n_factor);
    };

    if ( merging )
      return select;

    for ( auto& next: layers )
      asizes.push_back( next.dwSets == 0 ? next.pIndex->GetMaxIndex() : 0 );

    for ( size_t from = 0; from != asizes.size(); ++from )
      if ( asizes[from] != 0 )
        for ( size_t to = from + 1; to <= asizes.size() && asizes[to - 1] != 0; ++to )
          if ( to - from > 1 )
          {
            float crange = Ranker( from, to );

            if ( select.first == select.second || crange > srange || (crange == srange && size_t(to - from) > size_t(select.second - select.first)) )
            {
              select = { layers.begin() + from, layers.begin() + to };
              srange = crange;
            }
          }
    return select;
  }

// check if any events occured; process events first
// for each processed event, either reduce the index sent the event,
// or simply remove the empty index
// or process the errors
  auto  ContentsIndex::WaitGetEvent( const std::chrono::seconds& timeout ) -> EventRec
  {
    auto  exwait = mtc::make_unique_lock( evMutex );
    auto  fEvent = [this](){  return !canRun || !evQueue.empty();  };

    if ( evEvent.wait_for( exwait, timeout, fEvent ) && canRun )
    {
      auto  ev_get = evQueue.front();
        evQueue.pop_front();
      return ev_get;
    }
    return { nullptr, Notify::Event::None };
  }

  // Index implementation

  auto  Index::Set( mtc::api<IStorage> ps ) -> Index&
  {
    return contentsStorage = ps, *this;
  }

  auto Index::Set( const dynamic::Settings& settings ) -> Index&
  {
    return dynamicSettings = settings, *this;
  }

  auto Index::Create() -> mtc::api<IContentsIndex>
  {
    if ( contentsStorage == nullptr )
      throw std::logic_error( "layered index storage is not defined" );
    return (new ContentsIndex( contentsStorage, dynamicSettings ))->StartMonitor( runMonitorDelay );
  }

  auto  Index::Create( const mtc::api<IContentsIndex>* indices, size_t size ) -> mtc::api<IContentsIndex>
  {
    return new ContentsIndex( indices, size );
  }

  auto  Index::Create( const std::vector<mtc::api<IContentsIndex>>& indices ) -> mtc::api<IContentsIndex>
  {
    return Create( indices.data(), indices.size() );
  }

}}}
