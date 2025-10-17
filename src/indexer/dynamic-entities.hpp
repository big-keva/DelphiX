# if !defined( __DelphiX_src_indexer_dynamic_entities_hxx__ )
# define __DelphiX_src_indexer_dynamic_entities_hxx__
# include "../../contents.hpp"
# include "../../exceptions.hpp"
# include "../../primes.hpp"
# include "../../compat.hpp"
# include <mtc/ptrpatch.h>
# include <mtc/wcsstr.h>
# include <type_traits>
# include <stdexcept>
# include <vector>
# include <string>
# include <atomic>

namespace DelphiX {
namespace indexer {
namespace dynamic {

  template <class Allocator = std::allocator<char>>
  class EntityTable
  {
  public:
    class Iterator;

    class Entity: public IEntity, protected mtc::IByteBuffer
    {
      friend class EntityTable;

      using string = std::basic_string<char, std::char_traits<char>, AllocatorCast<Allocator, char>>;
      using extras = std::vector<char, AllocatorCast<Allocator, char>>;

    public:
      Entity( Allocator mm );

    public:     // from mtc::Iface
      long  Attach() override
        {  return ownerPtr != nullptr ? ownerPtr->Attach() : 1;  }
      long  Detach() override
        {  return ownerPtr != nullptr ? ownerPtr->Detach() : 0;  }

    public:     // overridables from IEntity
      auto  GetId() const -> EntityId override
        {  return { id, ownerPtr };  }
      auto  GetIndex() const -> uint32_t override
        {  return index;  }
      auto  GetExtra() const -> mtc::api<const IByteBuffer> override
        {  return this;  }
      auto  GetBundle() const -> mtc::api<const IByteBuffer> override
        {  return packPos != -1 && docStore != nullptr ? docStore->Get( packPos ) : nullptr;  }
      auto  GetVersion() const -> uint64_t override
        {  return version;  }

    protected:  // overridables from IByteBuffer
      auto  GetPtr() const noexcept -> const char* override
        {  return extra.data();  }
      auto  GetLen() const noexcept -> size_t override
        {  return extra.size();  }
      int   SetBuf( const void*, size_t ) override
        {  throw std::logic_error( "not implemented @" __FILE__ ":" LINE_STRING );  }
      int   SetLen( size_t ) override
        {  throw std::logic_error( "not implemented @" __FILE__ ":" LINE_STRING );  }

    public:     // customization
      auto  SetId( const EntityId& ) -> Entity&;
      auto  SetIndex( uint32_t ) -> Entity&;
      auto  SetOwner( Iface* ) -> Entity&;
      auto  SetStore( IStorage::IDumpStore* ) -> Entity&;
      auto  SetExtra( const StrView& ) -> Entity&;
      auto  SetPackPos( int64_t ) -> Entity&;
      auto  SetVersion( uint64_t ) -> Entity&;

      auto  GetPackPos() const -> int64_t {  return packPos;  }

    public:     // serialization
      template <class O>
      O*  Serialize( O* ) const;

    protected:
      string                id;                   // public entity id
      extras                extra;                // document attributes
      uint32_t              index = -1;          // order of creation, default 0
      int64_t               packPos = -1;
      uint64_t              version;

      mtc::Iface*           ownerPtr = nullptr;
      IStorage::IDumpStore* docStore = nullptr;
      std::atomic<Entity*>  collision = nullptr;  // relocation in the hash table

    };

  public:
    EntityTable( uint32_t size_limit, mtc::Iface* owner, IStorage::IDumpStore* store, Allocator alloc = Allocator() );
   ~EntityTable();

    auto  GetMaxEntities() const -> uint32_t  {  return uint32_t(entStore.size());  }
    auto  GetHashTableSize() const -> size_t  {  return entTable.size();  }

    auto  GetEntityCount() const -> uint32_t  {  return uint32_t(ptrStore.load() - (Entity*)entStore.data() - 1);  }

  public:
  /*
   *  ::GetEntity( ... )
   *  Get entity by entity index or id.
   *  Returns entity-api or nullptr if not found or deleted.
   */
    auto  GetEntity( uint32_t id ) -> mtc::api<Entity>  {  return getEntity( *this, id );  }
    auto  GetEntity( uint32_t id ) const -> mtc::api<const Entity>  {  return getEntity( *this, id );  }
    auto  GetEntity( const StrView& id ) -> mtc::api<Entity>  {  return getEntity( *this, id );  }
    auto  GetEntity( const StrView& id ) const -> mtc::api<const Entity>  {  return getEntity( *this, id );  }

  /*
   *  ::DelEntity( StrView )
   *  Delete entity by entity id.
   *  Returns false if not found.
   */
    auto  DelEntity( const StrView& ) -> uint32_t;

  /*
   *  ::SetEntity( StrView, uint32_t* deleted )
   *  Set new entity with new object id, replace if exists.
   *  Returns api and stores replaced id.
   *
   *  Throws index_overflow if more than accepted count of documents.
   */
    auto  SetEntity( const StrView&, const StrView& = {}, uint32_t* = nullptr ) -> mtc::api<Entity>;
    auto  SetExtras( const StrView&, const StrView& = {} ) -> mtc::api<Entity>;

  public:      // iterator access
    auto  GetIterator( uint32_t ) const -> Iterator;
    auto  GetIterator( const StrView& ) const -> Iterator;

  public:       // serialization
    template <class O>
    O*  Serialize( O* o ) const;

  protected:
    template <class Table>
    using EntityOf = typename std::conditional<std::is_const<Table>::value, const Entity, Entity>::type;

    auto  next_by_ix( uint32_t id ) const -> uint32_t;
    auto  next_by_id( uint32_t id ) const -> uint32_t;

  // access helpers
    auto  getEntity( uint32_t index ) const -> const Entity&
      {  return *(const Entity*)(index + entStore.data());  }
    auto  getEntity( uint32_t index )       -> Entity&
      {  return *(      Entity*)(index + entStore.data());  }

  // get implementation
    template <class S>
    static  auto  getEntity( S&, uint32_t ) -> mtc::api<EntityOf<S>>;
    template <class S>
    static  auto  getEntity( S&, const StrView& ) -> mtc::api<EntityOf<S>>;

  protected:
    using EntityHolder = typename std::aligned_storage<sizeof(Entity), alignof(Entity)>::type;
    using AtomicEntity = std::atomic<Entity*>;
    using EntityVector = std::vector<EntityHolder, AllocatorCast<Allocator, EntityHolder>>;
    using StrHashTable = std::vector<AtomicEntity, AllocatorCast<Allocator, AtomicEntity>>;

    const size_t EntityHolderSize = sizeof(EntityHolder);

    EntityVector   entStore;  // the entities storage
    AtomicEntity   ptrStore;
    StrHashTable   entTable;

    mtc::Iface*           ptrOwner = nullptr;
    IStorage::IDumpStore* docStore = nullptr;

  };

  template <class Allocator>
  class EntityTable<Allocator>::Iterator
  {
    friend class EntityTable;

    using Step = uint32_t  (EntityTable::*)( uint32_t ) const;

    const EntityTable*  entityTable = nullptr;
    uint32_t            entityIndex = uint32_t(-1);

    Step                goToNextOne = nullptr;

  protected:
    Iterator( const EntityTable& et, uint32_t ix, Step np ):
      entityTable( &et ),
      entityIndex( ix ),
      goToNextOne( np ) {}

  public:
    Iterator() = default;
    Iterator( const Iterator& ) = default;

  public:
    auto  Curr() -> mtc::api<const Entity>;
    auto  Next() -> mtc::api<const Entity>;

  };

  // EntityTable::Entiry implementation

  template <class Allocator>
  EntityTable<Allocator>::Entity::Entity( Allocator mm ):
    id( mm ), extra( mm ) {}

  template <class Allocator>
  auto  EntityTable<Allocator>::Entity::SetId( const EntityId& setid ) -> Entity&
    {  return id = setid, *this;  }

  template <class Allocator>
  auto  EntityTable<Allocator>::Entity::SetIndex( uint32_t ix ) -> Entity&
    {  return index = ix, *this;  }

  template <class Allocator>
  auto  EntityTable<Allocator>::Entity::SetOwner( Iface* po ) -> Entity&
    {  return ownerPtr = po, *this;  }

  template <class Allocator>
  auto  EntityTable<Allocator>::Entity::SetStore( IStorage::IDumpStore* ps ) -> Entity&
    {  return docStore = ps, *this;  }

  template <class Allocator>
  auto  EntityTable<Allocator>::Entity::SetExtra( const StrView& xtra ) -> Entity&
    {  return extra.insert( extra.end(), xtra.begin(), xtra.end() ), *this;  }

  template <class Allocator>
  auto  EntityTable<Allocator>::Entity::SetPackPos( int64_t tp ) -> Entity&
    {  return packPos = tp, *this;  }

  template <class Allocator>
  auto  EntityTable<Allocator>::Entity::SetVersion( uint64_t qw ) -> Entity&
    {  return version = qw, *this;  };

  template <class Allocator>
  template <class O>
  O*  EntityTable<Allocator>::Entity::Serialize( O* o ) const
  {
    return ::Serialize(
           ::Serialize(
           ::Serialize(
           ::Serialize(
           ::Serialize( o, index ), version ), packPos + 1 ), id ), extra );
  }

  // EntityTable implementation

  template <class Allocator>
  EntityTable<Allocator>::EntityTable( uint32_t size_limit, mtc::Iface* owner, IStorage::IDumpStore* store, Allocator alloc ):
    entStore( size_limit, alloc ),
    ptrStore( &getEntity( 1 ) ),
    entTable( UpperPrime( size_limit ), alloc ),
    ptrOwner( owner ),
    docStore( store )
  {
    new( entStore.data() )
      Entity( alloc );
  }

  template <class Allocator>
  EntityTable<Allocator>::~EntityTable()
  {
    for ( auto beg = &getEntity( 0 ), end = ptrStore.load(); beg != end; ++beg )
      beg->~Entity();
  }

 /*
  *  EntityTable::DelEntity( StrView id )
  *
  *  Marks the document with specified id as deleted and removes from the linked list
  *  if found. Returns false if document is not found.
  */
  template <class Allocator>
  auto  EntityTable<Allocator>::DelEntity( const StrView& id ) -> uint32_t
  {
    auto  hindex = std::hash<std::string_view>{}( { id.data(), id.size() } ) % entTable.size();
    auto* hentry = &entTable[hindex];
    auto  hvalue = mtc::ptr::clean( hentry->load() );
    auto  del_id = uint32_t(-1);

    // block the hash table entry from modifications
    while ( !hentry->compare_exchange_weak( hvalue, mtc::ptr::dirty( hvalue ) ) )
      hvalue = mtc::ptr::clean( hvalue );

    // search existing documents with same document id and remove
    for ( auto bfirst = true; hvalue != nullptr; hvalue = mtc::ptr::clean( hentry->load() ) )
      if ( StrView( hvalue->id ) == id )
      {
        if ( bfirst ) hentry->store( mtc::ptr::dirty( hvalue->collision.load() ) );
          else hentry->store( hvalue->collision.load() );

        std::swap( del_id, hvalue->index );
          break;
      }
        else
      {
        hentry = &hvalue->collision;
        bfirst = false;
      }

  // unblock the entry
    entTable[hindex].store( mtc::ptr::clean( entTable[hindex].load() ) );
    return del_id;
  }

  template <class Allocator>
  auto  EntityTable<Allocator>::SetEntity( const StrView& id, const StrView& xtras, uint32_t* deleted ) -> mtc::api<Entity>
  {
    auto  hindex = std::hash<std::string_view>{}( { id.data(), id.size() } ) % entTable.size();
    auto* hentry = &entTable[hindex];
    auto  hvalue = mtc::ptr::clean( hentry->load() );
    auto  entptr = ptrStore.load();

    if ( id.empty() )
      throw std::invalid_argument( "id is empty" );

    if ( deleted != nullptr )
      *deleted = uint32_t(-1);

  // ensure allocate space for the new document;
  // throw exception on unmodified table if could not allocate;
  // finish with entptr -> allocated entry
    for ( ;; )
    {
      if ( entptr >= &getEntity( 0 ) + entStore.size() )
        throw index_overflow( mtc::strprintf( "index size achieved limit of %d documents", entStore.size() ) );

      if ( !ptrStore.compare_exchange_weak( entptr, entptr + 1 ) )
        continue;

      (new( entptr ) Entity( entTable.get_allocator() ))->
        SetId( id ).
        SetIndex( uint32_t(entptr - &getEntity( 0 )) ).
        SetExtra( xtras ).
        SetOwner( ptrOwner ).
        SetStore( docStore );
      break;
    }

  // Ok, the entity is allocated and no changes made to document set and relocation table;
  // ensure the element may be created with docid == -1 meaning it is 'deleted'
  //
  // Now block the hash table entry from modifications outside and create reference to document
    while ( !hentry->compare_exchange_weak( hvalue, mtc::ptr::dirty( hvalue ) ) )
      hvalue = mtc::ptr::clean( hvalue );

  // search existing documents with same document id and remove
    for ( auto bfirst = true; hvalue != nullptr; hvalue = mtc::ptr::clean( hentry->load() ) )
      if ( StrView( hvalue->id ) == id )
      {
      // check algorithm consistency
        if ( hvalue->index == uint32_t(-1) )
          throw std::logic_error( "inconsistent lock-free algo @" __FILE__ ":" LINE_STRING );

      // exclude matching document from the list
        if ( bfirst ) hentry->store( mtc::ptr::dirty( hvalue->collision.load() ) );
          else hentry->store( hvalue->collision.load() );

      // check if deleted document index is requested
        if ( deleted != nullptr )
          *deleted = uint32_t(hvalue - &getEntity( 0 ));

      // mark excluded document as deleted
        hvalue->index = uint32_t(-1);
        break;
      }
        else
      {
        hentry = &hvalue->collision;
        bfirst = false;
      }

    // set up the document chain
    entptr->collision.store( mtc::ptr::clean( entTable[hindex].load() ) );
      entTable[hindex].store( entptr );

    return entptr;
  }

  template <class Allocator>
  auto  EntityTable<Allocator>::SetExtras( const StrView& id, const StrView& xtras ) -> mtc::api<Entity>
  {
    auto  hindex = std::hash<std::string_view>{}( { id.data(), id.size() } ) % entTable.size();
    auto& hentry = entTable[hindex];
    auto  hvalue = mtc::ptr::clean( hentry.load() );

    if ( id.empty() )
      throw std::invalid_argument( "id is empty" );

  // Now block the hash table entry from modifications outside
    while ( !hentry.compare_exchange_weak( hvalue, mtc::ptr::dirty( hvalue ) ) )
      hvalue = mtc::ptr::clean( hvalue );

  // search existing entity with specified id
    while ( hvalue != nullptr && StrView( hvalue->id ) != id )
      hvalue = mtc::ptr::clean( hvalue->collision.load() );

  // check if not found
    if ( hvalue == nullptr )
      return hentry.store( mtc::ptr::clean( hentry.load() ) ), nullptr;

  // set up the entity data
    try
    {
      hvalue->extra.resize( xtras.size() );
        memcpy( hvalue->extra.data(), xtras.data(), xtras.size() );

      return hentry.store( mtc::ptr::clean( hentry.load() ) ), hvalue;
    }
    catch ( ... )
    {
      hentry.store( mtc::ptr::clean( hentry.load() ) );
      throw;
    }
  }

  /*
   *  EntityTable::getEntity( uint32_t index )
   *
   *  Scans the list element by element, looking for matching document index.
   */
  template <class Allocator>
  template <class S>
  auto  EntityTable<Allocator>::getEntity( S& self, uint32_t index ) -> mtc::api<EntityOf<S>>
  {
    auto  entptr = decltype(&self.getEntity( index )){};

    if ( index == 0 || index == uint32_t(-1) || index >= self.entStore.size() )
      throw std::invalid_argument( "index out of range" );

    if ( (entptr = &self.getEntity( index )) >= self.ptrStore.load() )
      return nullptr;

    return entptr->index != uint32_t(-1) ? entptr : nullptr;
  }

  template <class Allocator>
  template <class S>
  auto  EntityTable<Allocator>::getEntity( S& self, const StrView& id ) -> mtc::api<EntityOf<S>>
  {
    auto  hindex = std::hash<std::string_view>{}( { id.data(), id.size() } ) % self.entTable.size();
    auto  docptr = mtc::ptr::clean( self.entTable[hindex].load() );

    // search matching document
    while ( docptr != nullptr && StrView( docptr->id ) != id )
      docptr = mtc::ptr::clean( docptr->collision.load() );

    // if found, lock and check if not deleted
    return docptr != nullptr && docptr->index != uint32_t(-1) ? docptr : nullptr;
  }

  template <class Allocator>
  auto  EntityTable<Allocator>::GetIterator( uint32_t id ) const -> Iterator
  {
    auto  beg = &getEntity( id );
    auto  end = ptrStore.load();

    while ( beg < end && beg->index == uint32_t(-1) )
      ++beg;

    return Iterator( *this, uint32_t(beg - &getEntity( 0 )), &EntityTable::next_by_ix );
  }

  template <class Allocator>
  auto  EntityTable<Allocator>::GetIterator( const StrView& id ) const -> Iterator
  {
    auto  minone = (const Entity*)nullptr;

  // locate minimal entity with id >= passed one
    for ( auto beg = &getEntity( 1 ), end = (const Entity*)ptrStore.load(); beg != end; ++beg )
      if ( beg->index != uint32_t(-1) && StrView( beg->id ) >= id )
        if ( minone == nullptr || beg->id < minone->id )
          minone = beg;

    return Iterator( *this, minone != nullptr ? uint32_t(minone - &getEntity( 0 )) : uint32_t(-1),
      &EntityTable::next_by_id );
  }

  template <class Allocator>
  template <class O>
  O*  EntityTable<Allocator>::Serialize( O* o ) const
  {
    auto  delEnt = Entity( entTable.get_allocator() );

    if ( (o = delEnt.Serialize( o )) == nullptr )
      return nullptr;

    for ( auto ptr = &getEntity( 1 ), end = (const Entity*)ptrStore.load(); ptr != end && o != nullptr; ++ptr )
      if ( ptr->index != uint32_t(-1) ) o = ptr->Serialize( o );
        else o = delEnt.Serialize( o );

    return o;
  }

  template <class Allocator>
  auto  EntityTable<Allocator>::next_by_ix( uint32_t id ) const -> uint32_t
  {
    auto  ptrbeg = &getEntity( 0 );
    auto  ptrend = ptrStore.load();

    for ( ++id; ptrbeg + id < ptrend && ptrbeg[id].index == uint32_t(-1); ++id )
      (void)NULL;

    return ptrbeg + id < ptrend ? id : uint32_t(-1);
  }

  template <class Allocator>
  auto  EntityTable<Allocator>::next_by_id( uint32_t id ) const -> uint32_t
  {
    auto  lastid = &getEntity( id ).id;
    auto  select = (const Entity*)nullptr;

    for ( auto beg = &getEntity( 1 ), end = (const Entity*)ptrStore.load(); beg != end; ++beg )
      if ( beg->index != uint32_t(-1) && beg->id > *lastid )
        if ( select == nullptr || beg->id < select->id )
          lastid = &(select = beg)->id;

    return select != nullptr ? select->index : uint32_t(-1);
  }

  // EntityTable::Iterator implementation

  template <class Allocator>
  auto  EntityTable<Allocator>::Iterator::Curr() -> mtc::api<const Entity>
  {
    auto  maxIndex = entityTable->ptrStore.load() - &entityTable->getEntity( 0 );

    while ( entityIndex < maxIndex && entityTable->getEntity( entityIndex ).index == uint32_t(-1) )
      ++entityIndex;

    return entityIndex < maxIndex ? &entityTable->getEntity( entityIndex ) : nullptr;
  }

  template <class Allocator>
  auto  EntityTable<Allocator>::Iterator::Next() -> mtc::api<const Entity>
  {
    entityIndex = (entityTable->*goToNextOne)( entityIndex );

    return entityIndex != uint32_t(-1) ? &entityTable->getEntity( entityIndex ) : nullptr;
  }

}}}

# endif   // __DelphiX_src_indexer_dynamic_entities_hxx__
