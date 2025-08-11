# if !defined( __DelphiX_context_index_keys_hpp__ )
# define __DelphiX_context_index_keys_hpp__
# include <mtc/serialize.h>
# include <mtc/wcsstr.h>
# include <stdexcept>
# include <cstdint>

namespace DelphiX {
namespace context {

  class Key
  {
    enum: unsigned
    {
      is_string = 1,
      has_class = 2
    };

    struct OnHeapKey
    {
      void  (*freeFn)( OnHeapKey* );
      int     rcount;
    };

    template <class Allocator>
    struct Allocated
    {
      Allocator   malloc;
      OnHeapKey   onHeap;

    public:
      Allocated( Allocator mem, void (*Delete)( OnHeapKey* ) ):
        malloc( mem ),
        onHeap{ Delete, 1 } {}
    };

    template <class Allocator> static
    constexpr auto  HeapBase( OnHeapKey* p ) -> Allocated<Allocator>*
    {
      const size_t offset = (const char*)&((Allocated<Allocator>*)nullptr)->onHeap
        - (const char*)(Allocated<Allocator>*)nullptr;
      return (Allocated<Allocator>*)((char*)p - offset);
    }

    template <class Allocator> static
    auto  AllocKey( size_t len, Allocator mem ) -> OnHeapKey*
    {
      auto  memman = typename std::allocator_traits<Allocator>::template rebind_alloc<
        Allocated<Allocator>>( mem );
      auto  nalloc = (len + 2 * sizeof(Allocated<Allocator>) - 1) / sizeof(Allocated<Allocator>);
      auto  palloc = new( memman.allocate( nalloc ) ) Allocated<Allocator>( mem, []( OnHeapKey* p )
        {
          auto  object = HeapBase<Allocator>( p );
          auto  memman = typename std::allocator_traits<Allocator>::template rebind_alloc<Allocated<Allocator>>
            ( object->malloc );
          memman.deallocate( object, 0 );
        } );
      return &palloc->onHeap;
    }

    char  keybuf[sizeof(char*) * 3];
    char* keyptr;

    template <class A>
    using char_string = std::basic_string<char, std::char_traits<char>, A>;
    template <class A>
    using wide_string = std::basic_string<widechar, std::char_traits<widechar>, A>;

    template <class I>
    static  int   valuelen( I );
    static  char* writeint( char*, uint16_t );
    static  char* writeint( char*, uint32_t );
    static  char* writeint( char*, uint64_t );
    static  auto  loadfrom( const char*, uint32_t& ) -> const char*;

  public:
    Key();
    Key( Key&& );
    Key( const Key& );
    Key( unsigned, uint32_t );
    Key( unsigned, uint64_t );
  template <class Allocator = std::allocator<char>>
    Key( unsigned, uint32_t, const widechar*, size_t, Allocator = Allocator() );
  template <class Allocator = std::allocator<char>>
    Key( unsigned, const widechar*, size_t, Allocator = Allocator() );
  template <class OtherAllocator, class Allocator = std::allocator<char>>
    Key( unsigned idl, uint32_t cls, const wide_string<OtherAllocator>& str, Allocator mem = Allocator() ):
      Key( idl, cls, str.data(), str.size(), mem ) {}
  template <class OtherAllocator, class Allocator = std::allocator<char>>
    Key( unsigned idl, const wide_string<OtherAllocator>& str, Allocator mem = Allocator() ):
      Key( idl, str.data(), str.size(), mem ) {}
  template <class Allocator = std::allocator<char>>
    Key( const void*, size_t, Allocator = Allocator() );
  template <class OtherAllocator, class Allocator = std::allocator<char>>
    Key( const char_string<OtherAllocator>& str, Allocator mem = Allocator() ):
      Key( str.data(), str.size(), mem )  {}
  template <class Allocator = std::allocator<char>>
    Key( const std::string_view& str, Allocator mem = Allocator() ):
      Key( str.data(), str.size(), mem )  {}
   ~Key();

    Key& operator=( Key&& ) noexcept;
    Key& operator=( const Key& );

    bool  operator==( const Key& key ) const  {  return compare( key ) == 0; }
    bool  operator!=( const Key& key ) const  {  return !(*this == key); }
    bool  operator<( const Key& key ) const   {  return compare( key ) < 0; }
    bool  operator<=( const Key& key ) const  {  return compare( key ) <= 0; }
    bool  operator>( const Key& key ) const   {  return compare( key ) > 0; }
    bool  operator>=( const Key& key ) const  {  return compare( key ) >= 0; }

    int   compare( const Key& ) const;

  public:
    void  clear();

    auto  data() const -> const char*
    {
      return keyptr != nullptr ?
        ::SkipToEnd( (const char*)keyptr, (unsigned*)nullptr ) : nullptr;
    }
    auto  size() const -> size_t
    {
      unsigned  cchkey;

      return keyptr != nullptr ?
        ::FetchFrom( (const char*)keyptr, cchkey ), cchkey : 0;
    }

    bool  has_int() const;
    bool  has_str() const;
    bool  has_cls() const;

    auto  get_idl() const -> unsigned;
    auto  get_int() const -> uint64_t;
    auto  get_cls() const -> uint32_t;
    auto  get_str( widechar*, size_t ) const -> const widechar*;
    auto  get_len() const -> size_t;

  };

  // basic_key template implementation

  template <class U>
  int   Key::valuelen( U u )
  {
    return u <= 0x0000007f ? 1 :
           u <= 0x000007ff ? 2 :
           u <= 0x0000ffff ? 3 :
           u <= 0x001fffff ? 4 :
           u <= 0x03ffffff ? 5 :
           u <= 0x7fffffff ? 6 :
           u <= 0x0fffffffff ? 7 :
           u <= 0x03ffffffffff ? 8 : (throw std::invalid_argument( "value too big to be presented as utf-8" ), 0);
  }

  template <class Allocator>
  Key::Key( unsigned idl, uint32_t cls, const widechar* str, size_t len, Allocator mem )
  {
    auto    keylen = valuelen( (idl << 2) | is_string | has_class ) + valuelen( cls ) + valuelen( 0 );
    size_t  nalloc;
    char*   outptr = keyptr = keybuf;

    if ( str == nullptr || len == 0 )
      throw std::invalid_argument( "key string is empty" );

    if ( len == size_t(-1) )
      for ( len = 0; str[len] != 0; ++len ) (void)NULL;

    for ( auto s = str; s != str + len; ++s )
      keylen += valuelen( *s );

    if ( (nalloc = ::GetBufLen( keylen ) + keylen) > sizeof(keybuf) )
      outptr = keyptr = (char*)(1 + AllocKey( nalloc, mem ));

    for ( outptr = writeint( ::Serialize( outptr, keylen ), (idl << 2)  | is_string | has_class ); len-- != 0; ++str )
      outptr = writeint( outptr, *str );

    writeint( writeint( outptr, uint16_t(0) ), cls );
  }

  template <class Allocator>
  Key::Key( unsigned idl, const widechar* str, size_t len, Allocator mem )
  {
    auto    keylen = valuelen( (idl << 2) | is_string ) + valuelen( 0 );
    size_t  nalloc;
    char*   outptr = keyptr = keybuf;

    if ( str == nullptr || len == 0 )
      throw std::invalid_argument( "key string is empty" );

    if ( len == size_t(-1) )
      for ( len = 0; str[len] != 0; ++len ) (void)NULL;

    for ( auto s = str; s != str + len; ++s )
      keylen += valuelen( *s );

    if ( (nalloc = ::GetBufLen( keylen ) + keylen) > sizeof(keybuf) )
      outptr = keyptr = (char*)(1 + AllocKey( nalloc, mem ));

    for ( outptr = writeint( ::Serialize( outptr, keylen ), (idl << 2) | is_string ); len-- != 0; ++str )
      outptr = writeint( outptr, *str );

    writeint( outptr, uint16_t(0) );
  }

  template <class Allocator>
  Key::Key( const void* data, size_t size, Allocator mem )
  {
    auto  keylen = ::GetBufLen( size ) + size;

    if ( sizeof(keybuf) < keylen )
      keyptr = (char*)(1 + AllocKey( keylen, mem ));
    else keyptr = keybuf;

    memcpy( ::Serialize( keyptr, size ), data, size );
  }

}}

# endif // __DelphiX_context_index_keys_hpp__
