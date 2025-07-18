# include "textAPI/DOM-load.hpp"
# include <mtc/json.h>

namespace DelphiX {
namespace textAPI {
namespace load_as {

  class FnStream: public mtc::json::parse::stream
  {
    std::function<char()> reader;
  public:
    FnStream( std::function<char()> fr ):
      reader( fr )  {}
    char  get() override
      {  return reader();  }
  };

  void  jsonString( mtc::api<IText> doc, mtc::json::parse::reader& src );
  void  jsonVector( mtc::api<IText> doc, mtc::json::parse::reader& src );
  void  jsonStruct( mtc::api<IText> doc, mtc::json::parse::reader& src );
  void  jsonRecord( mtc::api<IText> doc, mtc::json::parse::reader& src );

  auto  jsonString( mtc::json::parse::reader& src ) -> std::string
  {
    char  buf[1024];
    char* ptr = buf;
    auto  str = std::string();

    if ( src.nospace() != '\"' )
      throw ParseError( "'\"' expected" );

    for ( auto bslash = false; ; )
    {
      auto  chr = src.getnext();

      if ( !bslash && chr == '\\' )
      {
        bslash = true;
        continue;
      }

      switch ( chr )
      {
        case '\0':
          throw ParseError( "unexpected end of input" );
        case '"':
          if ( !bslash )
            return str + std::string( buf, ptr - buf );
          break;
        case '/':
          break;
        case 'b':
          chr = bslash ? '\b' : chr;
          break;
        case 'f':
          chr = bslash ? '\f' : chr;
          break;
        case 'n':
          chr = bslash ? '\n' : chr;
          break;
        case 'r':
          chr = bslash ? '\r' : chr;
          break;
        case 't':
          chr = bslash ? '\t' : chr;
          break;
        default: break;
      }
      bslash = false;

      if ( ptr == buf + sizeof(buf) )
      {
        str += std::string( buf, ptr - buf );
        ptr = buf;
      }
      *ptr++ = chr;
    }
  }

  void  jsonString( mtc::api<IText> doc, mtc::json::parse::reader& src )
  {
    doc->AddString( jsonString( src ), codepages::codepage_utf8 );
  }

  void  jsonVector( mtc::api<IText> doc, mtc::json::parse::reader& src )
  {
    char  chr;

    if ( (chr = src.nospace()) != '[' )
      throw ParseError( "'[' expected" );

    while ( (chr = src.nospace()) != ']' )
    {
      switch ( chr )
      {
        case '\"':  jsonString( doc, src.putback( chr ) );  break;
        case '{':   jsonStruct( doc, src.putback( chr ) );  break;
        default:    throw ParseError( mtc::strprintf( "expected character '%c'", chr ) );
      }
      if ( (chr = src.nospace()) != ',' )
        src.putback( chr );
    }
  }

  void  jsonStruct( mtc::api<IText> doc, mtc::json::parse::reader& src )
  {
    char  chr;

    if ( (chr = src.nospace()) != '{' )
      throw ParseError( "'{' expected" );

    while ( (chr = src.nospace()) != '}' )
    {
      if ( chr == '\"' )
      {
        auto  key = jsonString( src.putback( chr ) );
        auto  tag = doc->AddTextTag( key.c_str(), key.length() );

        if ( src.nospace() == ':' )
          jsonRecord( tag, src );
        else throw ParseError( "':' expected" );

        if ( (chr = src.nospace()) != '}' )
          throw ParseError( mtc::strprintf( "unexpected '%c', multiple tags not supported", chr ) );
        else src.putback( chr );
      } else throw ParseError( "tag name expected" );
    }
  }

  void  jsonRecord( mtc::api<IText> doc, mtc::json::parse::reader& src )
  {
    auto  chr = src.nospace();

    switch ( chr )
    {
      case '"': return jsonString( doc, src.putback( chr ) );
      case '[': return jsonVector( doc, src.putback( chr ) );
      case '{': return jsonStruct( doc, src.putback( chr ) );
      default:  throw ParseError( mtc::strprintf( "expected character '%c'", chr ) );
    }
  }

  void  jsonVector( mtc::api<IText> doc, mtc::json::parse::stream& src )
  {
    auto  reader = mtc::json::parse::reader( src );

    return jsonVector( doc, reader );
  }

  void  Json( mtc::api<IText> doc, std::function<char()> src )
  {
    FnStream  stm( src );

    return jsonVector( doc, stm );
  }

}}}
