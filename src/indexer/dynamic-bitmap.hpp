# if !defined( __DelphiX_src_indexer_dynamic_bitmap_hxx__ )
# define __DelphiX_src_indexer_dynamic_bitmap_hxx__
# include "../../compat.hpp"
# include <stdexcept>
# include <climits>
# include <atomic>

namespace DelphiX {
namespace indexer {

  template <class Allocator = std::allocator<char>>
  class Bitmap
  {
    struct vector_data: std::vector<std::atomic_uint32_t, AllocatorCast<Allocator, std::atomic_uint32_t>>
    {
      vector_data( size_t count, Allocator alloc ):
        std::vector<std::atomic<uint32_t>, AllocatorCast<Allocator, std::atomic_uint32_t>>( count, alloc ) {}

      long  refcount = 1;
    };

    vector_data*  bitmap = nullptr;

    enum: size_t
    {
      element_size = sizeof(uint32_t),
      element_bits = element_size * CHAR_BIT
    };

    void  detach();

  public:
    Bitmap( size_t maxSize, Allocator alloc = Allocator() );
    Bitmap( const Bitmap& );
    Bitmap( Bitmap&& );
   ~Bitmap();
    Bitmap& operator=( const Bitmap& );
    Bitmap& operator=( Bitmap&& );

  public:
    void  Set( uint32_t );
    bool  Get( uint32_t ) const;
  };

  // Bitmap template implementation

  template <class Allocator>
  Bitmap<Allocator>::Bitmap( size_t maxSize, Allocator alloc )
  {
    bitmap = new( AllocatorCast<Allocator, vector_data>( alloc ).allocate( 1 ) )
      vector_data( (maxSize + element_bits - 1) / element_bits, alloc );
  }

  template <class Allocator>
  Bitmap<Allocator>::Bitmap( const Bitmap& bim )
  {
    if ( (bitmap = bim.bitmap) != nullptr )
      ++bitmap->refcount;
  }

  template <class Allocator>
  Bitmap<Allocator>::Bitmap( Bitmap&& bim ):
    bitmap( bim.bitmap )
  {
    bim.bitmap = nullptr;
  }

  template <class Allocator>
  Bitmap<Allocator>::~Bitmap()
  {
    detach();
  }

  template <class Allocator>
  Bitmap<Allocator>& Bitmap<Allocator>::operator=( const Bitmap& bim )
  {
    detach();
    if ( (bitmap = bim.bitmap) != nullptr )
      ++bitmap->refcount;
    return *this;
  }

  template <class Allocator>
  Bitmap<Allocator>& Bitmap<Allocator>::operator=( Bitmap&& bim )
  {
    detach();
    if ( (bitmap = bim.bitmap) != nullptr )
      bim.bitmap = nullptr;
    return *this;
  }

  template <class Allocator>
  void  Bitmap<Allocator>::detach()
  {
    if ( bitmap != nullptr && --bitmap->refcount == 0 )
    {
      auto  alloc = AllocatorCast<Allocator, vector_data>(
        bitmap->get_allocator() );
      bitmap->~vector_data();
        alloc.deallocate( bitmap, 0 );
    }
  }

  template <class Allocator>
  void  Bitmap<Allocator>::Set( uint32_t uvalue )
  {
    auto  uindex = size_t(uvalue / element_bits);
    auto  ushift = uvalue % element_bits;
    auto  ddmask = (1 << ushift);

    if ( bitmap != nullptr && uindex < bitmap->size() )
    {
      auto& item = bitmap->at( uindex );

      for ( auto uval = item.load(); !item.compare_exchange_weak( uval, uval | ddmask ); )
        (void)NULL;
    } else throw std::range_error( "entity index exceeds deleted map capacity" );
  }

  template <class Allocator>
  bool  Bitmap<Allocator>::Get( uint32_t uvalue ) const
  {
    auto  uindex = size_t(uvalue / element_bits);
    auto  ushift = uvalue % element_bits;
    auto  ddmask = (1 << ushift);

    if ( bitmap != nullptr && uindex < bitmap->size() )
      return (bitmap->at( uindex ).load() & ddmask) != 0;

    return false;
  }

}}

# endif   // __DelphiX_src_indexer_dynamic_bitmap_hxx__
