# include "../../indexer/dynamic-contents.hpp"
# include "override-entities.hpp"
# include "dynamic-entities.hpp"
# include "dynamic-chains.hpp"
# include "../../exceptions.hpp"
# include <mtc/arena.hpp>

namespace DelphiX {
namespace indexer {
namespace dynamic {

  class ContentsIndex final: public IContentsIndex
  {
    using Allocator = mtc::Arena::allocator<char>;

    using EntTable = EntityTable<Allocator>;
    using Contents = BlockChains<Allocator>;

    implement_lifetime_control

    class KeyValue;
    class Entities;
    class EntitiesList;
    class ContentsList;

  public:
    ContentsIndex(
      uint32_t maxEntities,
      uint32_t maxAllocate, mtc::api<IStorage::IIndexStore> outputStorage );
   ~ContentsIndex() {  contents.StopIt();  }

  public:
    auto  GetEntity( EntityId ) const -> mtc::api<const IEntity> override;
    auto  GetEntity( uint32_t ) const -> mtc::api<const IEntity> override;

    bool  DelEntity( EntityId ) override;
    auto  SetEntity( EntityId, mtc::api<const IContents>,
      const StrView&, const StrView& ) -> mtc::api<const IEntity> override;
    auto  SetExtras( EntityId,
      const StrView& ) -> mtc::api<const IEntity> override;

    auto  GetMaxIndex() const -> uint32_t override  {  return entities.GetEntityCount();  }
    auto  GetKeyBlock( const StrView& ) const -> mtc::api<IEntities> override;
    auto  GetKeyStats( const StrView& ) const -> BlockInfo override;

    auto  ListEntities( EntityId ) -> mtc::api<IEntitiesList> override;
    auto  ListEntities( uint32_t ) -> mtc::api<IEntitiesList> override;
    auto  ListContents( const StrView& ) -> mtc::api<IContentsList> override;

    auto  Commit() -> mtc::api<IStorage::ISerialized> override;
    auto  Reduce() -> mtc::api<IContentsIndex> override  {  return this;  }
    void  Remove() override;

    void  Stash( EntityId ) override  {  throw std::logic_error( "not implemented @" __FILE__ ":" LINE_STRING );  }

  protected:
    const uint32_t                  memLimit;
    mtc::Arena                      memArena;

    mtc::api<IStorage::IIndexStore> pStorage;

    EntTable                        entities;
    Contents                        contents;
    Bitmap<Allocator>               shadowed;

  };

  class ContentsIndex::KeyValue: public IIndexAPI
  {
    Contents& contents;
    uint32_t  entityId;

  public:
    KeyValue( Contents& cts, uint32_t ent ):
      contents( cts ),
      entityId( ent ) {}
    auto  ptr() const -> IIndexAPI* {  return (IIndexAPI*)this;  }

  public:
    void  Insert( const StrView& key, const StrView& value, unsigned bkType ) override
      {  return contents.Insert( key, entityId, value, bkType );  }

  };

  class ContentsIndex::Entities final: public IEntities
  {
    friend class ContentsIndex;

    using ChainHook = std::remove_pointer<decltype((
      contents.Lookup({})))>::type;
    using ChainLink = std::remove_pointer<decltype((
      contents.Lookup({})->pfirst.load()))>::type;

    implement_lifetime_control

  protected:
    Entities( ChainHook* chain, const ContentsIndex* owner ):
      pwhere( chain ),
      pchain( mtc::ptr::clean( chain->pfirst.load() ) ),
      parent( owner ) {}

  public:     // IEntities overridables
    auto  Find( uint32_t ) -> Reference override;
    auto  Type() const -> uint32_t override {  return pwhere->bkType;  }
    auto  Size() const -> uint32_t override {  return pwhere->ncount.load();  }

  protected:
    ChainHook*                    pwhere;
    ChainLink*                    pchain;
    mtc::api<const ContentsIndex> parent;

  };

  class ContentsIndex::EntitiesList final: public IEntitiesList
  {
    implement_lifetime_control

  public:
    EntitiesList( EntTable::Iterator&& it, ContentsIndex* pc ):
      contents( pc ),
      iterator( std::move( it ) ) {}

  public:
    auto  Curr() -> mtc::api<const IEntity> override  {  return iterator.Curr().ptr();  }
    auto  Next() -> mtc::api<const IEntity> override  {  return iterator.Next().ptr();  }

  protected:
    mtc::api<ContentsIndex> contents;
    EntTable::Iterator      iterator;

  };

  class ContentsIndex::ContentsList final: public IContentsList
  {
    implement_lifetime_control

  public:
    ContentsList( ContentsIndex* ix, const StrView& tp ):
      contents( ix ),
      iterator( contents->contents.KeySet( tp ) ) {}

  public:
    auto  Curr() -> std::string override  {  return iterator.CurrentKey();  }
    auto  Next() -> std::string override  {  return iterator.GetNextKey();  }

  protected:
    mtc::api<ContentsIndex> contents;
    Contents::KeyLister     iterator;

  };

  // ContentsIndex implementation

  ContentsIndex::ContentsIndex( uint32_t maxEntities, uint32_t maxAllocate, mtc::api<IStorage::IIndexStore> storageSink  ):
    memLimit( maxAllocate ),
    pStorage( storageSink ),
    entities( maxEntities, this, pStorage != nullptr ? pStorage->Packages() : nullptr, memArena.get_allocator<char>() ),
    contents( memArena.get_allocator<char>() ),
    shadowed( maxEntities, memArena.get_allocator<char>() )
  {
  }

  auto  ContentsIndex::GetEntity( EntityId id ) const -> mtc::api<const IEntity>
  {
    return entities.GetEntity( id ).ptr();
  }

  auto  ContentsIndex::GetEntity( uint32_t id ) const -> mtc::api<const IEntity>
  {
    return entities.GetEntity( id ).ptr();
  }

  bool  ContentsIndex::DelEntity( EntityId id )
  {
    uint32_t  del_id = entities.DelEntity( id );

    return del_id != (uint32_t)-1 ? shadowed.Set( del_id ), true : false;
  }

  auto  ContentsIndex::SetEntity( EntityId id, mtc::api<const IContents> keys,
    const StrView& xtra, const StrView& beef ) -> mtc::api<const IEntity>
  {
    auto  entity = mtc::api<EntTable::Entity>();
    auto  bodies = pStorage != nullptr ? pStorage->Packages() : nullptr;
    auto  del_id = uint32_t{};
    auto  bdlPos = int64_t(-1);

  // check memory requirements
    if ( memArena.memusage() > memLimit )
      throw index_overflow( "dynamic index memory overflow" );

  // check if bodies are defined
    if ( bodies != nullptr && !beef.empty() )
      bdlPos = bodies->Put( beef.data(), beef.size() );

  // create the entity
    entity = entities.SetEntity( id, xtra, &del_id );
      entity->SetPackPos( bdlPos );

  // check if any entities deleted
    if ( del_id != uint32_t(-1) )
      shadowed.Set( del_id );

  // process contents indexing
    if ( keys != nullptr )
      keys->Enum( KeyValue( contents, entity->GetIndex() ).ptr() );

    return Override::Entity( entity.ptr() ).Bundle( bodies, entity->GetPackPos() );
  }

  auto  ContentsIndex::SetExtras( EntityId id, const StrView& extras ) -> mtc::api<const IEntity>
  {
    return entities.SetExtras( id, extras ).ptr();
  }

  auto  ContentsIndex::GetKeyBlock( const StrView& key ) const -> mtc::api<IEntities>
  {
    auto  pchain = contents.Lookup( { key.data(), key.size() } );

    return pchain != nullptr && pchain->pfirst.load() != nullptr ?
      new Entities( pchain, this ) : nullptr;
  }

  auto  ContentsIndex::GetKeyStats( const StrView& key ) const -> BlockInfo
  {
    auto  pchain = contents.Lookup( { key.data(), key.size() } );

    if ( pchain != nullptr )
      return { pchain->bkType, pchain->ncount };
    return { uint32_t(-1), 0 };
  }

  auto  ContentsIndex::ListEntities( EntityId id ) -> mtc::api<IEntitiesList>
  {
    return new EntitiesList( entities.GetIterator( id ), this );
  }

  auto  ContentsIndex::ListEntities( uint32_t ix ) -> mtc::api<IEntitiesList>
  {
    return new EntitiesList( entities.GetIterator( ix ), this );
  }

  auto  ContentsIndex::ListContents( const StrView& key ) -> mtc::api<IContentsList>
  {
    return new ContentsList( this, key );
  }

  auto  ContentsIndex::Commit() -> mtc::api<IStorage::ISerialized>
  {
    if ( pStorage == nullptr )
      throw std::logic_error( "output storage is not defined, but FlushSink() was called" );

  // finalize keys thread and remove all the deleted elements from lists
    contents.StopIt().Remove( shadowed );

//    contents.VerifyIds( GetMaxIndex() );

  // store entities table
    entities.Serialize( pStorage->Entities().ptr() );
    contents.Serialize( pStorage->Contents().ptr(), pStorage->Linkages().ptr() );

    return pStorage->Commit();
  }

  void  ContentsIndex::Remove()
  {
    if ( pStorage != nullptr )
      pStorage->Remove();
  }

  // ContentsIndex::Entities implemenation

  auto  ContentsIndex::Entities::Find( uint32_t id ) -> Reference
  {
    while ( pchain != nullptr && (pchain->entity < id || parent->shadowed.Get( pchain->entity )) )
      pchain = pchain->p_next.load();

    if ( pchain != nullptr )
      return { pchain->entity, { pchain->data(), pchain->lblock } };
    else
      return { uint32_t(-1), { nullptr, 0 } };
  }

  // contents implementation

  auto  Index::Set( const Settings& options ) -> Index&
    {  return openOptions = options, *this;  }

  auto  Index::Set( mtc::api<IStorage::IIndexStore> storage ) -> Index&
    {  return storageSink = storage, *this;  }

  auto  Index::Create() const -> mtc::api<IContentsIndex>
    {  return new ContentsIndex( openOptions.maxEntities, openOptions.maxAllocate, storageSink );  }

}}}
