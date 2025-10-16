# include "../../context/index-keys.hpp"

namespace DelphiX {
namespace context {

  // Key implementation

  char* Key::writeint( char* p, uint16_t n )
  {
    if ( (n & ~0x007f) == 0 )
    {
      *p++ = (char)(n & 0x7f);
    }
      else
    if ( (n & ~0x07ff) == 0 )
    {
      *p++ = 0xc0 | ((char)(n >> 0x06));
      *p++ = 0x80 | ((char)(n & 0x3f));
    }
      else
    {
      *p++ = 0xe0 | ((char)(n >> 0x0c));
      *p++ = 0x80 | ((char)(n >> 0x06) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x00) & 0x3f);
    }
    return p;
  }

  char* Key::writeint( char* p, uint32_t n )
  {
    if ( (n & ~0x0000ffff) == 0 )
      return writeint( p, (uint16_t)n );
    if ( (n & ~0x001fffff) == 0 )
    {
      *p++ = 0xf0 | ((char)(n >> 0x12));
      *p++ = 0x80 | ((char)(n >> 0x0c) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x06) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x00) & 0x3f);
    }
      else
    if ( (n & ~0x03ffffff) == 0 )
    {
      *p++ = 0xf8 | ((char)(n >> 0x18));
      *p++ = 0x80 | ((char)(n >> 0x12) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x0c) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x06) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x00) & 0x3f);
    }
      else
    if ( (n & ~0x7fffffff) == 0 )
    {
      *p++ = 0xfc | ((char)(n >> 0x1e));
      *p++ = 0x80 | ((char)(n >> 0x18) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x12) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x0c) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x06) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x00) & 0x3f);
    }
      else
    {
      *p++ = (char)0xfe;
      *p++ = 0x80 | ((char)(n >> 0x1e) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x18) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x12) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x0c) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x06) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x00) & 0x3f);
    }
    return p;
  }

  char* Key::writeint( char* p, uint64_t n )
  {
    if ( (n & ~uint64_t(0x0ffffffff)) == 0 )
      return writeint( p, (uint32_t)n );
    if ( (n & ~uint64_t(0x0fffffffff)) == 0 )
    {
      *p++ = (char)0xfe;
      *p++ = 0x80 | ((char)(n >> 0x1e) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x18) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x12) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x0c) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x06) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x00) & 0x3f);
    }
      else
    if ( (n & ~uint64_t(0x03ffffffffff)) == 0 )
    {
      *p++ = (char)0xff;
      *p++ = 0x80 | ((char)(n >> 0x24) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x1e) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x18) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x12) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x0c) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x06) & 0x3f);
      *p++ = 0x80 | ((char)(n >> 0x00) & 0x3f);
    }
      else
    throw std::invalid_argument( "value too big to be presented as utf-8" );
    return p;
  }

  auto  Key::loadfrom( const char* p, uint32_t& v ) -> const char*
  {
    uint8_t   ctlchr = (uint8_t)*p++;

    if ( ctlchr == 0xfe )
    {
      v = ((uint32_t)(uint8_t)(p[0] & 0x3f)) << 0x1e
        | ((uint32_t)(uint8_t)(p[1] & 0x3f)) << 0x18
        | ((uint32_t)(uint8_t)(p[2] & 0x3f)) << 0x12
        | ((uint32_t)(uint8_t)(p[3] & 0x3f)) << 0x0c
        | ((uint32_t)(uint8_t)(p[4] & 0x3f)) << 0x06
        | ((uint32_t)(uint8_t)(p[5] & 0x3f));
      return p + 6;
    }

    if ( (ctlchr & 0xfe) == 0xfc )
    {
      v = (ctlchr & 0x01) << 0x1e
        | ((uint32_t)(uint8_t)(p[0] & 0x3f)) << 0x18
        | ((uint32_t)(uint8_t)(p[1] & 0x3f)) << 0x12
        | ((uint32_t)(uint8_t)(p[2] & 0x3f)) << 0x0c
        | ((uint32_t)(uint8_t)(p[3] & 0x3f)) << 0x06
        | ((uint32_t)(uint8_t)(p[4] & 0x3f));
      return p + 5;
    }

    if ( (ctlchr & 0xfc) == 0xf8 )
    {
      v = (ctlchr & 0x03) << 0x18
        | ((uint32_t)(uint8_t)(p[0] & 0x3f)) << 0x12
        | ((uint32_t)(uint8_t)(p[1] & 0x3f)) << 0x0c
        | ((uint32_t)(uint8_t)(p[2] & 0x3f)) << 0x06
        | ((uint32_t)(uint8_t)(p[3] & 0x3f));
      return p + 4;
    }

    if ( (ctlchr & 0xf8) == 0xf0 )
    {
      v = (ctlchr & 0x07) << 0x12
        | ((uint32_t)(uint8_t)(p[0] & 0x3f)) << 0x0c
        | ((uint32_t)(uint8_t)(p[1] & 0x3f)) << 0x06
        | ((uint32_t)(uint8_t)(p[2] & 0x3f));
      return p + 3;
    }

    if ( (ctlchr & 0xf0) == 0xe0 )
    {
      v = (ctlchr & 0x0f) << 0x0c
        | ((uint32_t)(uint8_t)(p[0] & 0x3f)) << 0x06
        | ((uint32_t)(uint8_t)(p[1] & 0x3f));
      return p + 2;
    }

    if ( (ctlchr & 0xe0) == 0xc0 )
    {
      v = (ctlchr & 0x1f) << 0x06
        | ((uint32_t)(uint8_t)(*p & 0x3f));
      return p + 1;
    }

    if ( (ctlchr & 0x80) == 0 )
    {
      return (v = ctlchr), p;
    }

    return nullptr;
  }

  Key::Key(): keyptr( nullptr )
  {
  }

  Key::Key( Key&& key )
  {
    if ( (keyptr = key.keyptr) != nullptr )
    {
      if ( keyptr == key.keybuf )
        keyptr = (char*)memcpy( keybuf, key.keybuf, sizeof(keybuf) );
      key.keyptr = nullptr;
    }
  }

  Key::Key( const Key& key )
  {
    if ( (keyptr = key.keyptr) != nullptr )
    {
      if ( keyptr != key.keybuf ) ++(-1 + (OnHeapKey*)keyptr)->rcount;
        else keyptr = (char*)memcpy( keybuf, key.keybuf, sizeof(keybuf) );
    }
  }

  Key::Key( unsigned idl, uint32_t lex )
  {
    auto  keylen = valuelen( idl << 2 ) + valuelen( lex );
    auto  keyout = keyptr = keybuf;

    writeint( writeint( ::Serialize( keyout, keylen ), idl << 2 ), lex );
  }

  Key::Key( unsigned idl, uint64_t lex )
  {
    auto  keylen = valuelen( idl << 2 ) + valuelen( lex );
    auto  keyout = keyptr = keybuf;

    writeint( writeint( ::Serialize( keyout, keylen ), idl << 2 ), lex );
  }

  Key::~Key()
  {
    clear();
  }

  auto  Key::operator=( Key&& key ) noexcept -> Key&
  {
    clear();

    if ( (keyptr = key.keyptr) != nullptr )
    {
      if ( keyptr == key.keybuf )
        keyptr = (char*)memcpy( keybuf, key.keybuf, sizeof(keybuf) );
      key.keyptr = nullptr;
    }
    return *this;
  }

  auto  Key::operator=( const Key& key ) -> Key&
  {
    clear();

    if ( (keyptr = key.keyptr) != nullptr )
    {
      if ( keyptr == key.keybuf ) ++(-1 + (OnHeapKey*)keyptr)->rcount;
        else keyptr = (char*)memcpy( keybuf, key.keybuf, sizeof(keybuf) );
    }
    return *this;
  }

  int   Key::compare( const Key& key ) const
  {
    if ( keyptr != key.keyptr )
    {
      auto  src = (const uint8_t*)data();
      auto  cmp = (const uint8_t*)key.data();
      auto  end = src + std::min( size(), key.size() );
      int   res;

      while ( src != end && (res = *src - *cmp) == 0 )
        ++src, ++cmp;

      return res != 0 ? res : (size() > key.size()) - (size() < key.size());
    }
    return 0;

  }

  void  Key::clear()
  {
    if ( keyptr != nullptr && keyptr != keybuf )
    {
      auto  memkey = ((OnHeapKey*)keyptr) - 1;

      if ( --memkey->rcount == 0 )
        memkey->freeFn( memkey );
    }
    keyptr = nullptr;
  }

  bool  Key::has_int() const
  {
    uint32_t  idl;

    if ( keyptr != nullptr )
    {
      return loadfrom( ::SkipToEnd( (const char*)keyptr, (int*)nullptr ), idl ) != nullptr ?
        (idl & (is_string | has_class)) == 0 : false;
    }
    return false;
  }

  bool  Key::has_str() const
  {
    uint32_t  idl;

    if ( keyptr != nullptr )
    {
      return loadfrom( ::SkipToEnd( (const char*)keyptr, (int*)nullptr ), idl ) != nullptr ?
        (idl & is_string) != 0 : false;
    }
    return false;
  }

  bool  Key::has_cls() const
  {
    uint32_t  idl;

    if ( keyptr != nullptr )
    {
      return loadfrom( ::SkipToEnd( (const char*)keyptr, (int*)nullptr ), idl ) != nullptr ?
        (idl & has_class) != 0 : false;
    }
    return false;
  }

  auto  Key::get_idl() const -> unsigned
  {
    uint32_t  idl;

    if ( keyptr != nullptr )
      if ( loadfrom( ::SkipToEnd( (const char*)keyptr, (int*)nullptr ), idl ) != nullptr )
        return idl >> 2;

    return unsigned(-1);
  }

  auto  Key::get_int() const -> uint64_t
  {
    uint32_t    idl;
    uint32_t    lex;
    const char* src;

    if ( keyptr != nullptr )
      if ( (src = loadfrom( ::SkipToEnd( (const char*)keyptr, (int*)nullptr ), idl )) != nullptr )
        if ( (idl & (is_string | has_class)) == 0 && loadfrom( src, lex ) != nullptr )
          return lex;

    return 0;
  }

  auto  Key::get_cls() const -> uint32_t
  {
    uint32_t    idl;
    uint32_t    cls;
    const char* src;

    if ( keyptr == nullptr )
      return 0;

    if ( (src = loadfrom( ::SkipToEnd( (const char*)keyptr, (int*)nullptr ), idl )) == nullptr )
      return 0;

    if ( (idl & has_class) == 0 )
      return 0;

    if ( (idl & is_string) != 0 )
      while ( (src = loadfrom( src, cls )) != nullptr && cls != 0 )
        (void)NULL;

    return src != nullptr && (src = loadfrom( src, cls )) != nullptr ? cls : 0;
  }

  auto  Key::get_str( widechar* out, size_t max ) const -> const widechar*
  {
    uint32_t    idl;
    size_t      len;
    uint32_t    chr;
    const char* src;

    // check the arguments
    if ( out == nullptr || max == 0 )
      throw std::invalid_argument( "string buffer is empty" );

    // check if has anything
    if ( keyptr == nullptr )
      return nullptr;

    // check if has string
    if ( (src = loadfrom( ::SkipToEnd( (const char*)keyptr, (int*)nullptr ), idl )) != nullptr )
      if ( (idl & is_string) == 0 )
        return nullptr;

    // get string
    for ( len = 0; len != max && (src = loadfrom( src, chr )) != nullptr && chr != 0; )
      out[len++] = chr;

    return src != nullptr && len < max ? out[len] = 0, out : nullptr;
  }

  auto  Key::get_len() const -> size_t
  {
    uint32_t    idl;
    size_t      len;
    unsigned    chr;
    const char* src;

    if ( keyptr == nullptr )
      return 0;

  // check if has string
    if ( (src = loadfrom( ::SkipToEnd( (const char*)keyptr, (int*)nullptr ), idl )) != nullptr )
      if ( (idl & is_string) == 0 )
        return 0;

  // get string length
    for ( len = 0; (src = loadfrom( src, chr )) != nullptr && chr != 0; ++len )
      (void)NULL;

    return src != nullptr ? len : 0;
  }

}}
