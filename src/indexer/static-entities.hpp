# if !defined( __DelphiX_src_indexer_static_entities_hxx__ )
# define __DelphiX_src_indexer_static_entities_hxx__
# include "../../contents.hpp"
# include "../../primes.hpp"
# include "../../macros.hpp"
# include <mtc/ptrpatch.h>
# include <stdexcept>
# include <algorithm>

namespace DelphiX {
namespace indexer {
namespace static_ {

  template <class Allocator = std::allocator<char>>
  class EntityTable
  {
  public:
    class Region final: public mtc::IByteBuffer
    {
      implement_lifetime_control

    public:
      Region( const char* p, size_t l, const mtc::Iface* o ):
        bufptr( p ),
        length( l ),
        iOwner( o ) {}

    public:
      auto  GetPtr() const -> const char* override
        {  return bufptr;  }
      auto  GetLen() const -> size_t override
        {  return length;  }
      int   SetBuf( const void*, size_t ) override
        {  throw std::logic_error( "not implemented @" __FILE__ ":" LINE_STRING );  }
      int   SetLen( size_t ) override
        {  throw std::logic_error( "not implemented @" __FILE__ ":" LINE_STRING );  }

    protected:
      const char*                 bufptr;
      size_t                      length;
      mtc::api<const mtc::Iface>  iOwner;
    };

    class Entity: public IEntity
    {
      friend class EntityTable;

      using string = std::basic_string<char, std::char_traits<char>, Allocator>;

    public:
      Entity( mtc::Iface* owner, IStorage::IDumpStore* dumps ):
        owner_ptr( owner ),
        dumpStore( dumps ){}

    public:
      long  Attach() override
        {  return owner_ptr != nullptr ? owner_ptr->Attach() : 1;  }
      long  Detach() override
        {  return owner_ptr != nullptr ? owner_ptr->Detach() : 0;  }

    // overridables from IEntity
      auto  GetId() const -> EntityId override
        {  return { entity_id, owner_ptr };  }
      auto  GetIndex() const -> uint32_t override
        {  return index;  }
      auto  GetExtra() const -> mtc::api<const mtc::IByteBuffer> override
        {  return new Region( extras.data(), extras.size(), owner_ptr );  }
      auto  GetBundle() const -> mtc::api<const mtc::IByteBuffer> override
        {  return packPos != -1 && dumpStore != nullptr ? dumpStore->Get( packPos ) : nullptr;  }
      auto  GetVersion() const -> uint64_t override
        {  return version;  }

      bool  ValidIndex() const noexcept {  return index != 0 && index != uint32_t(-1);  }
      auto  GetPackPos() const -> int64_t  {  return packPos;  }

    protected:
    // loading
      auto  FetchFrom( const char* ) -> const char*;

    public:
      Iface*                owner_ptr = nullptr;
      Entity*               collision = nullptr;
      IStorage::IDumpStore* dumpStore = nullptr;

      StrView               entity_id;
      StrView               extras;
      uint32_t              index = 0;            // order of creation, default 0
      int64_t               packPos = -1;
      uint64_t              version = 0;

    };

    class Iterator;

  protected:
    using vector_type = std::vector<Entity, Allocator>;
    using hash_vector = std::vector<Entity*, Allocator>;

  public:
    EntityTable( const StrView&, mtc::Iface*, IStorage::IDumpStore*, Allocator = Allocator() );
   ~EntityTable();

    auto  GetEntityCount() const -> uint32_t {  return std::max( 1U, uint32_t( entityTable.size() ) ) - 1;  };

  // entities access
    auto  GetEntity( uint32_t id ) const -> mtc::api<const Entity>;
    auto  GetEntity( const StrView& id ) const -> mtc::api<const Entity>;

  // iterator
    auto  GetIterator( uint32_t ) const -> Iterator;
    auto  GetIterator( const StrView& ) const -> Iterator;

  protected:
    auto  getKeyIndex() const -> const EntityTable&;
    auto  getNextByIx( uint32_t id ) const -> uint32_t;
    auto  getNextById( uint32_t id ) const -> uint32_t;

  protected:
    using IndexByKeys = std::vector<uint32_t, Allocator>;

    mtc::Iface*                             contentsPtr = nullptr;
    vector_type                             entityTable;
    hash_vector                             entitiesMap;
    mutable std::atomic<IndexByKeys*>       indexByKeys = nullptr;

  };

  template <class Allocator>
  class EntityTable<Allocator>::Iterator
  {
    friend class EntityTable;

    using FnNext = uint32_t  (EntityTable::*)( uint32_t ) const;

    const EntityTable&  parent;
    FnNext              fnNext;
    uint32_t            uindex;

  protected:
    Iterator( const EntityTable& table, FnNext fnext, uint32_t index ):
      parent( table ),
      fnNext( fnext ),
      uindex( index ) {}

  public:     // construction
    Iterator( const Iterator& ) = default;
    Iterator& operator=( const Iterator& ) = default;

  public:     // iterator properties
    auto  Curr() -> const Entity*;
    auto  Next() -> const Entity*;

  };

  // EntityTable::Entity implementation

  template <class Allocator>
  auto  EntityTable<Allocator>::Entity::FetchFrom( const char* src ) -> const char*
  {
    unsigned  cch;

  // get index and id length; set entity id
    if ( (src = ::FetchFrom( ::FetchFrom( ::FetchFrom( ::FetchFrom( src,
      index ), version ), packPos ), cch )) == nullptr ) return nullptr;

    src = (entity_id = { src, cch }).data() + cch;
    --packPos;

    if ( packPos == -1 && index != uint32_t(-1) )
    {
      int i = 0;
    }

  // get attribute length
    if ( (src = ::FetchFrom( src, cch )) == nullptr )
      return nullptr;

    return src = (extras = { src, cch }).data() + cch;
  }

  // EntityTable implementation

  template <class Allocator>
  EntityTable<Allocator>::EntityTable( const StrView& input, mtc::Iface* owner, IStorage::IDumpStore* dumps, Allocator alloc ):
    contentsPtr( owner ),
    entityTable( alloc ),
    entitiesMap( alloc )
  {
    entityTable.reserve( input.size() / 0x40 );

  // load the table
    for ( auto src = input.data(), end = input.data() + input.size(); src != nullptr && src != end; )
    {
      entityTable.emplace_back( owner, dumps );
      src = entityTable.back().FetchFrom( src );
    }

  // allocate hash table
    entitiesMap.resize( UpperPrime( entityTable.size() ) );

    for ( auto& next: entityTable )
      if ( next.index != 0 && next.index != uint32_t(-1) )
      {
        auto  hash = std::hash<std::string_view>{}( { next.entity_id.data(), next.entity_id.size() } );
        auto  offs = hash % entitiesMap.size();

        next.collision = entitiesMap[offs];
        entitiesMap[offs] = &next;
      }
  }

  template <class Allocator>
  EntityTable<Allocator>::~EntityTable()
  {
    auto   pindex = mtc::ptr::clean( indexByKeys.load() );

    while ( pindex != nullptr && !indexByKeys.compare_exchange_weak( pindex, mtc::ptr::dirty( pindex ) ) )
      pindex = mtc::ptr::clean( pindex );

    if ( pindex != nullptr )
    {
      auto  malloc = typename std::allocator_traits<Allocator>::rebind_alloc<IndexByKeys>(
        entityTable.get_allocator() );

      pindex->~IndexByKeys();
      malloc.deallocate( pindex, 0 );
    }
  }

  template <class Allocator>
  auto  EntityTable<Allocator>::GetEntity( uint32_t id ) const -> mtc::api<const Entity>
  {
    return id > 0 && id < entityTable.size() ? &entityTable[id] : nullptr;
  }

  template <class Allocator>
  auto  EntityTable<Allocator>::GetEntity( const StrView& id ) const -> mtc::api<const Entity>
  {
    if ( id.empty() )
      throw std::invalid_argument( "empty entity id" );

    if ( entitiesMap.size() != 0 )
    {
      auto  hash = std::hash<std::string_view>{}( { id.data(), id.size() } );
      auto  offs = hash % entitiesMap.size();

      for ( auto next = entitiesMap[offs]; next != nullptr; next = next->collision )
        if ( next->entity_id == id )
          return next;
    }

    return nullptr;
  }

  template <class Allocator>
  auto  EntityTable<Allocator>::GetIterator( uint32_t id ) const -> Iterator
  {
    for ( ; id < entityTable.size(); ++id )
    {
      if ( entityTable[id].ValidIndex() )
        return Iterator( *this, &EntityTable::getNextByIx, id );
    }
    return Iterator( *this, &EntityTable::getNextByIx, uint32_t(-1) );
  }

  template <class Allocator>
  auto  EntityTable<Allocator>::GetIterator( const StrView& id ) const -> Iterator
  {
    auto  pindex = getKeyIndex().indexByKeys.load();
    auto  pfound = std::lower_bound( pindex->begin(), pindex->end(), id, [&]( uint32_t i, const StrView& id )
      {  return entityTable[i].entity_id < id;  } );

    while ( pfound != pindex->end() && !entityTable[*pfound].ValidIndex() )
      ++pfound;

    return Iterator( *this, &EntityTable::getNextById, pfound != pindex->end() ?
      *pfound : uint32_t(-1) );
  }

  template <class Allocator>
  auto  EntityTable<Allocator>::getKeyIndex() const -> const EntityTable&
  {
    auto  pindex = mtc::ptr::clean( indexByKeys.load() );

    for ( ; ; )
    {
      if ( pindex != nullptr )
        return *this;

      if ( indexByKeys.compare_exchange_weak( pindex, mtc::ptr::dirty( pindex ) ) )
        if ( (pindex = mtc::ptr::clean( pindex )) == nullptr )
        {
          auto  malloc = typename std::allocator_traits<Allocator>::rebind_alloc<IndexByKeys>(
            entityTable.get_allocator() );

        // allocate new table
          pindex = new( malloc.allocate( 1 ) )
            IndexByKeys( malloc );
          pindex->reserve( entityTable.size() - 1 );

        // fill table with document indices
          for ( auto& next: entityTable )
            if ( next.index != 0 && next.index != uint32_t(-1) )
              pindex->push_back( next.index );

        // sort ids by entity id
          std::sort( pindex->begin(), pindex->end(), [&]( uint32_t lhs, uint32_t rhs )
          {
            return entityTable[lhs].entity_id < entityTable[rhs].entity_id;
          } );

          return indexByKeys = pindex, *this;
        }
    }
  }

  template <class Allocator>
  auto  EntityTable<Allocator>::getNextByIx( uint32_t id ) const -> uint32_t
  {
    if ( id != uint32_t(-1) )
    {
      for ( ++id; id != entityTable.size() && !entityTable[id].ValidIndex(); ++id )
        (void)NULL;
      if ( id == entityTable.size() )
        id = uint32_t(-1);
    }
    return id;
  }

  template <class Allocator>
  auto  EntityTable<Allocator>::getNextById( uint32_t id ) const -> uint32_t
  {
    if ( id != uint32_t(-1) )
    {
      auto  pindex = indexByKeys.load();
      auto  pfound = std::find( pindex->begin(), pindex->end(), id );

      if ( pfound != pindex->end() )
        ++pfound;

      while ( pfound != pindex->end() && !entityTable[*pfound].ValidIndex() )
        ++pfound;

      if ( pfound != pindex->end() )
        return *pfound;
    }
    return uint32_t(-1);
  }

  // EntityTable::Iterator implementation

  template <class Allocator>
  auto  EntityTable<Allocator>::Iterator::Curr() -> const Entity*
  {
    return uindex != uint32_t(-1) ? &parent.entityTable[uindex] : nullptr;
  }

  template <class Allocator>
  auto  EntityTable<Allocator>::Iterator::Next() -> const Entity*
  {
    if ( uindex != uint32_t(-1) )
    {
      if ( (uindex = (parent.*fnNext)( uindex )) != uint32_t(-1) )
        return &parent.entityTable[uindex];
    }
    return nullptr;
  }

}}}

# endif   // __DelphiX_src_indexer_static_entities_hxx__
