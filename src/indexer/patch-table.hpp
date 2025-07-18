# include "contents.hpp"
# include "slices.hpp"
# include "primes.hpp"
# include <mtc/ptrpatch.h>
# include <string_view>
# include <cstring>
# include <vector>
# include <atomic>
# include <stdexcept>
# include <mtc/serialize.h>

namespace DelphiX {
namespace indexer {

  template <class Allocator = std::allocator<char>>
  class PatchTable
  {
    class PatchVal;
    class PatchRec;

    using HashItem = std::atomic<PatchRec*>;

    template <class Target>
    using MakeAllocator = typename std::allocator_traits<Allocator>::rebind_alloc<Target>;

  public:
    PatchTable( size_t maxPatches, Allocator alloc = Allocator() );
   ~PatchTable();

  public:
    auto  Update( const StrView& id, uint32_t ix, const StrView& md ) -> mtc::api<const mtc::IByteBuffer>;
    void  Delete( const StrView& id, uint32_t ix );
    auto  Search( const StrView& ) const -> mtc::api<const mtc::IByteBuffer>;
    auto  Search( uint32_t ix ) const -> mtc::api<const mtc::IByteBuffer>
      {  return Search( MakeId( ix ) );  }

    void  Commit( mtc::api<IStorage::ISerialized> );

  protected:
    auto  Modify( const StrView&, const mtc::api<const mtc::IByteBuffer>& ) -> mtc::api<const mtc::IByteBuffer>;

  protected:
    static  auto  MakeId( uint32_t ix ) -> StrView;
    static  auto  HashId( const StrView& ) -> size_t;
    static  bool  IsUint( const StrView& );

  protected:
    std::atomic_long                  modifiers = 0;
    std::vector<HashItem, Allocator>  hashTable;

  };

  template <class Allocator>
  class PatchTable<Allocator>::PatchVal final: public mtc::IByteBuffer  // record keeping the serialized value
  {
    friend class PatchTable;

    using PatchAlloc = MakeAllocator<PatchVal>;

    struct deleted_t {};

    constexpr static deleted_t deleted{};

  protected:
    PatchVal( const StrView&, Allocator );
    PatchVal( const deleted_t&, Allocator );

  public:
    static
    auto  Create( const StrView&, Allocator ) -> mtc::api<const IByteBuffer>;
    static
    auto  Create( const deleted_t&, Allocator ) -> mtc::api<const IByteBuffer>;

  public:
    long  Attach() override {  return ++rcount;  }
    long  Detach() override;

  public:
    auto  GetPtr() const -> const char* override
      {  return length != size_t(-1) ? (char*)(this + 1) : nullptr;  }
    auto  GetLen() const -> size_t
      {  return length;  }
    int   SetBuf( const void*, size_t ) override
      {  throw std::logic_error( "not implemented" );  }
    int   SetLen( size_t ) override
      {  throw std::logic_error( "not implemented" );  }

  protected:
    PatchAlloc        memman;
    std::atomic_long  rcount = 0;
    size_t            length;
  };

  template <class Allocator>
  class PatchTable<Allocator>::PatchRec
  {
    friend class PatchTable;

  public:
    PatchRec( PatchRec* ptr, const StrView& key, const mtc::IByteBuffer* val, long ver ):
      collision( ptr ),
      entityKey( key ),
      patchData( val ),
      patchTime( ver )
    {
      if ( val != nullptr )
        const_cast<mtc::IByteBuffer*>( val )->Attach();
    }

  public:
    // collision resolving comparison
    bool  operator == ( const StrView& to ) const;
    bool  operator != ( const StrView& to ) const  {  return !(*this == to);  }

    // helper func
    auto  Modify( mtc::api<const mtc::IByteBuffer> ) -> mtc::api<const mtc::IByteBuffer>;

  protected:
    std::atomic<PatchRec*>                collision = nullptr;
    StrView                                  entityKey = { nullptr, 0 };
    std::atomic<const mtc::IByteBuffer*>  patchData = nullptr;
    std::atomic_long                      patchTime = 0;

  };

  // PatchVal implementation

  template <class Allocator>
  PatchTable<Allocator>::PatchVal::PatchVal( const StrView& data, Allocator mman ):
    memman( mman ),
    length( data.size() )
  {
    memcpy( (void*)PatchVal::GetPtr(), data.data(), length );
  }

  template <class Allocator>
  PatchTable<Allocator>::PatchVal::PatchVal( const deleted_t&, Allocator mman ):
    memman( mman ),
    length( -1 )
  {
  }

  template <class Allocator>
  auto  PatchTable<Allocator>::PatchVal::Create( const StrView& data, Allocator mman ) -> mtc::api<const mtc::IByteBuffer>
  {
    auto  nalloc = (sizeof(PatchVal) * 2 + data.size() - 1) / sizeof(PatchVal);
    auto  palloc = new( MakeAllocator<PatchVal>( mman ).allocate( nalloc ) )
      PatchVal( data, mman );

    return palloc;
  }

  template <class Allocator>
  auto  PatchTable<Allocator>::PatchVal::Create( const PatchVal::deleted_t&, Allocator mman ) -> mtc::api<const mtc::IByteBuffer>
  {
    return new( MakeAllocator<PatchVal>( mman ).allocate( 1 ) )
      PatchVal( PatchVal::deleted, mman );
  }

  template <class Allocator>
  long  PatchTable<Allocator>::PatchVal::Detach()
  {
    auto  refcount = --rcount;

    if ( refcount == 0 )
    {
      this->~PatchVal();
      memman.deallocate( this, 0 );
    }

    return refcount;
  }

  // PatchTable::PatchRec implementation

  template <class Allocator>
  bool  PatchTable<Allocator>::PatchRec::operator == ( const StrView& to ) const
  {
    if ( entityKey.data() == to.data() )
      return true;

    if ( (uintptr_t(to.data()) & uintptr_t(entityKey.data())) >> 32 == uint32_t(-1) )
      return uint32_t(uintptr_t(entityKey.data())) == uint32_t(uintptr_t(to.data()));

    if ( (uintptr_t(entityKey.data()) >> 32) == uint32_t(-1) || (uintptr_t(to.data()) >> 32) == uint32_t(-1) )
      return false;

    return std::string_view( entityKey.data(), entityKey.size() )
        == std::string_view( to.data(), to.size() );
  }

 /*
  * PatchTable<Allocator>::PatchRec::Modify( data )
  *
  * Set the PatchVal data to value passed.
  * If there is an existing patch data, updates it.
  * Does not override deleted data.
  */
  template <class Allocator>
  auto  PatchTable<Allocator>::PatchRec::Modify( mtc::api<const mtc::IByteBuffer> data ) -> mtc::api<const mtc::IByteBuffer>
  {
    auto  pvalue = mtc::ptr::clean( patchData.load() );
    auto  result = data;

  // get and lock current patch value
    while ( !patchData.compare_exchange_weak( pvalue, mtc::ptr::dirty( pvalue ) ) )
      pvalue = mtc::ptr::clean( pvalue );

  // check if current value is valid
    if ( pvalue == nullptr )
    {
      patchData.store( pvalue );
      throw std::logic_error( "invalid PatchRec value == NULL" );
    }

  // check if existing document is already deleted; ignore value passed
  // and return current value
    if ( pvalue->GetLen() == size_t(-1) )
      return (result = pvalue), patchData.store( pvalue ), result;

  // override current patchData
    const_cast<mtc::IByteBuffer*>( pvalue )->Detach();

    return patchData.store( data.release() ), result;
  }

  // PatchTable implementation
  template <class Allocator>
  PatchTable<Allocator>::PatchTable( size_t maxPatches, Allocator alloc ):
    hashTable( UpperPrime( maxPatches ), alloc )
  {
  }

  template <class Allocator>
  PatchTable<Allocator>::~PatchTable()
  {
    auto  entryAlloc = MakeAllocator<PatchRec>( hashTable.get_allocator() );

    for ( auto& rentry: hashTable )
      for ( auto pentry = mtc::ptr::clean( rentry.load() ); pentry != nullptr; )
      {
        auto  pfetch = pentry->collision.load();
        auto  pvalue = pentry->patchData.load();

        if ( pvalue != nullptr )
          const_cast<mtc::IByteBuffer*>( pvalue )->Detach();

        pentry->~PatchRec();
        entryAlloc.deallocate( pentry, 0 );

        pentry = pfetch;
      }
  }

  /*
  * Creates a metadata update record with no override for 'deleted'
  */
  template <class Allocator>
  auto  PatchTable<Allocator>::Modify( const StrView& key, const mtc::api<const mtc::IByteBuffer>& pvalue ) -> mtc::api<const mtc::IByteBuffer>
  {
    auto& rentry = hashTable[HashId( key ) % hashTable.size()];
    auto  pentry = mtc::ptr::clean( rentry.load() );

    // check if delete existing element
    for ( ; pentry != nullptr; pentry = mtc::ptr::clean( pentry->collision.load() ) )
      if ( *pentry == key )
        return pentry->Modify( pvalue );

    // try lock the hash entry
    for ( pentry = mtc::ptr::clean( rentry.load() ); !rentry.compare_exchange_weak( pentry, mtc::ptr::dirty( pentry ) ); )
      pentry = mtc::ptr::clean( pentry );

    // lookup the deletion record again
    for ( ; pentry != nullptr; pentry = mtc::ptr::clean( pentry->collision.load() ) )
      if ( *pentry == key )
      {
        rentry.store( mtc::ptr::clean( rentry.load() ) );
        return pentry->Modify( pvalue );
      }

    // allocate entry record and store to table
    try
    {
      pentry = new ( MakeAllocator<PatchRec>( hashTable.get_allocator() ).allocate( 1 ) )
        PatchRec( mtc::ptr::clean( rentry.load() ), key, pvalue.ptr(), ++modifiers );
      rentry.store( pentry );
      return pvalue;
    }
    catch ( ... )
    {
      rentry.store( pentry );
      throw;
    }
  }

  template <class Allocator>
  auto  PatchTable<Allocator>::Update( const StrView& id, uint32_t ix, const StrView& md ) -> mtc::api<const mtc::IByteBuffer>
  {
    auto  value = PatchVal::Create( md, hashTable.get_allocator() );
      Modify( id, value );
      Modify( MakeId( ix ), value );
    return value;
  }

  template <class Allocator>
  void  PatchTable<Allocator>::Delete( const StrView& id, uint32_t ix )
  {
    auto  value = PatchVal::Create( PatchVal::deleted, hashTable.get_allocator() );
      Modify( id,           value );
      Modify( MakeId( ix ), value );
    return (void)value;
  }

  template <class Allocator>
  auto  PatchTable<Allocator>::Search( const StrView& key ) const -> mtc::api<const mtc::IByteBuffer>
  {
    auto& rentry = hashTable[HashId( key ) % hashTable.size()];
    auto  pentry = mtc::ptr::clean( rentry.load() );

  // check if element exists
    for ( ; pentry != nullptr; pentry = mtc::ptr::clean( pentry->collision.load() ) )
      if ( *pentry == key )
        return pentry->patchData.load();

    return {};
  }

  template <class Allocator>
  void  PatchTable<Allocator>::Commit( mtc::api<IStorage::ISerialized> serial )
  {
    if ( serial == nullptr )
      throw std::invalid_argument( "empty PatchTable::Commit storage argument" );

    for ( auto modClock = 0L, curClock = modifiers.load(); modClock < modifiers.load(); modClock = curClock )
    {
      auto  ipatch = decltype(serial->NewPatch()){};

      for ( auto& next: hashTable )
      {
        auto  ppatch = next.load();   // PatchRec*

      // skip if record is empty, is already saved, or is integer index
        if ( ppatch != nullptr && !IsUint( ppatch->entityKey ) && ppatch->patchTime > modClock )
        {
          auto  pvalue = mtc::ptr::clean( ppatch->patchData.load() );    // const mtc::IByteBuffer*
          auto  locked = mtc::api<const mtc::IByteBuffer>();

        // lock the variable
          while ( !ppatch->patchData.compare_exchange_weak( pvalue, mtc::ptr::dirty( pvalue ) ) )
            pvalue = mtc::ptr::clean( pvalue );

        // get locked value and restore the lock
          ppatch->patchData.store( (locked = pvalue).ptr() );

        // ensure patch storage
          if ( ipatch == nullptr )
            ipatch = serial->NewPatch();

          if ( locked->GetLen() == size_t(-1) )
          {
            ipatch->Delete( { ppatch->entityKey.data(), ppatch->entityKey.size() } );
          }
            else
          {
            ipatch->Update( { ppatch->entityKey.data(), ppatch->entityKey.size() },
              locked->GetPtr(), locked->GetLen() );
          }
        }
      }

      if ( ipatch != nullptr )
        ipatch->Commit();
    }
  }

  template <class Allocator>
  auto  PatchTable<Allocator>::HashId( const StrView& key ) -> size_t
  {
    static_assert( sizeof(uintptr_t) > sizeof(uint32_t),
      "this code is designed for at least 64-bit pointers" );

    if ( (uintptr_t(key.data()) >> 32) == uint32_t(-1) )
      return uintptr_t(key.data()) & 0xffffffffUL;

    return std::hash<std::string_view>()( { key.data(), key.size() } );
  }

  template <class Allocator>
  bool  PatchTable<Allocator>::IsUint( const StrView& key )
  {
    return (uintptr_t(key.data()) >> 32) == uint32_t(-1);
  }

  template <class Allocator>
  auto  PatchTable<Allocator>::MakeId( uint32_t ix ) -> StrView
  {
    static_assert( sizeof(uintptr_t) > sizeof(uint32_t),
      "this code is designed for at least 64-bit pointers" );
    return StrView( (const char*)(0xffffffff00000000LU | ix), 0 );
  }

}}
