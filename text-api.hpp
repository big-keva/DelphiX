# if !defined( __DelphiX_textAPI_text_api_hpp__ )
# define __DelphiX_textAPI_text_api_hpp__
# include "slices.hpp"
# include <moonycode/codes.h>
# include <mtc/interfaces.h>
# include <mtc/serialize.h>
# include <stdexcept>
# include <cstdint>

namespace DelphiX {
namespace textAPI {

  template <class Format>
  struct FormatTag
  {
    static_assert( std::is_integral<Format>::value
      || mtc::class_is_string<Format>::value
      || std::is_same<Format, const char*>::value
      || std::is_same<Format, const widechar*>::value, "invalid tag type, strings and integers allowed" );

    Format      format;
    uint32_t    uLower;     // start offset, bytes
    uint32_t    uUpper;     // end offset, bytes

  public:
    bool  operator==( const FormatTag& to ) const
      {  return format == to.format && uLower == to.uLower && uUpper == to.uUpper;  }
    bool  operator!=( const FormatTag& to ) const
      {  return !(*this == to);  }
    bool  Covers( uint32_t pos ) const
      {  return pos >= uLower && pos <= uUpper;  }
    bool  Covers( const std::pair<uint32_t, uint32_t>& span ) const
      {  return uLower <= span.first && uUpper >= span.second;  }
  };

  template <> inline
  bool  FormatTag<const char*>::operator == ( const FormatTag& to ) const
  {
    return strcmp( format, to.format ) == 0 && uLower == to.uLower && uUpper == to.uUpper;
  }

  using MarkupTag = FormatTag<const char*>;
  using RankerTag = FormatTag<unsigned>;

  struct TextToken
  {
    enum: unsigned
    {
      lt_space = 1,
      is_punct = 2,
      is_first = 4
    };

    unsigned        uFlags;
    const widechar* pwsstr;
    uint32_t        offset;
    uint32_t        length;

    auto  GetWideStr() const -> std::basic_string_view<widechar>  {  return { pwsstr, length };  }
    auto  LeftSpaced() const {  return (uFlags & lt_space) != 0;  }
    auto  IsPointing() const {  return (uFlags & is_punct) != 0;  }
    auto  IsFirstOne() const {  return (uFlags & is_first) != 0;  }

  };

  struct TextChunk
  {
    const void* strptr;
    size_t      length;
    unsigned    encode;

    auto  GetCharStr() const -> std::basic_string_view<char>;
    auto  GetWideStr() const -> std::basic_string_view<widechar>;

  };

  struct IText: public mtc::Iface
  {
    virtual auto  AddTextTag( const char*, size_t = -1 ) -> mtc::api<IText> = 0;
    virtual void  AddCharStr( const char*, size_t, unsigned ) = 0;
    virtual void  AddWideStr( const widechar*, size_t ) = 0;

  protected:
    template <class A>
    using char_string = std::basic_string<char, std::char_traits<char>, A>;
    template <class A>
    using wide_string = std::basic_string<widechar, std::char_traits<widechar>, A>;

  public:     // wrappers
    auto  AddMarkup( const std::string_view& ) -> mtc::api<IText>;
    void  AddString( const std::string_view&, unsigned = codepages::codepage_utf8 );
    void  AddString( const std::basic_string_view<widechar>& );

    template <class Allocator>
    void  AddString( const char_string<Allocator>&, unsigned = codepages::codepage_utf8 );
    template <class Allocator>
    void  AddString( const wide_string<Allocator>& );

  };

  struct ITextView: public mtc::Iface
  {
    virtual auto  GetBlocks() const -> Slice<const TextChunk> = 0;
    virtual auto  GetMarkup() const -> Slice<const MarkupTag> = 0;
    virtual auto  GetLength() const -> uint32_t = 0;
  };

  bool  IsEncoded( const ITextView&, unsigned codepage );
  auto  CopyUtf16( IText*, const ITextView&, unsigned encode = codepages::codepage_utf8 ) -> IText*;
  auto  Serialize( IText*, const ITextView& ) -> IText*;

  inline
  bool  operator == ( const TextToken& t1, const TextToken& t2 )
  {
    if ( t1.uFlags == t2.uFlags && t1.length == t2.length )
      for ( auto p1 = t1.pwsstr, p2 = t2.pwsstr, pe = p1 + t1.length; p1 != pe; ++p1, ++p2 )
        if ( *p1 != *p2 )
          return false;
    return true;
  }

  // TextChunk implementation

  inline  auto  TextChunk::GetCharStr() const -> std::basic_string_view<char>
  {
    if ( encode == unsigned(-1) )
      throw std::invalid_argument( "invalid encoding" );
    return { (const char*)strptr, length };
  }

  inline  auto  TextChunk::GetWideStr() const -> std::basic_string_view<widechar>
  {
    if ( encode != unsigned(-1) )
      throw std::invalid_argument( "invalid encoding" );
    return { (const widechar*)strptr, length };
  }

  // IText implementation

  inline  auto  IText::AddMarkup( const std::string_view& str ) -> mtc::api<IText>
  {
    return AddTextTag( str.data(), str.size() );
  }

  inline  void  IText::AddString( const std::string_view& str, unsigned encoding )
  {
    return AddCharStr( str.data(), str.length(), encoding );
  }

  inline  void  IText::AddString( const std::basic_string_view<widechar>& str )
  {
    return AddWideStr( str.data(), str.length() );
  }

  template <class Allocator>
  void  IText::AddString( const char_string<Allocator>& s, unsigned encoding )
  {
    return AddString( { s.c_str(), s.size() }, encoding );
  }

  template <class Allocator>
  void  IText::AddString( const wide_string<Allocator>& s )
  {
    return AddWideStr( s.c_str(), s.size() );
  }

}}

# endif // !__DelphiX_textAPI_text_api_hpp__
