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

  struct Tag
  {
    mtc::api<IText> tag;
    std::string     key;
  };

  auto  trimString( std::string& str ) -> std::string&
  {
    auto  pos = str.find_last_not_of( " \t" );

    if ( pos != std::string::npos )
      str.resize( pos + 1 );

    return str;
  }

  void  loadObject( std::vector<Tag>& tagStack, mtc::json::parse::reader& src )
  {
    for ( ; !tagStack.empty(); )
    {
      auto  str = std::string();
      auto  chr = src.nospace();

      if ( chr == '\0' )
        break;

    // check control character; it may be either opening or closing tag;
    // extract tag name and the closing attribute
      if ( chr == '<' )
      {
        bool  closing;

        if ( (chr = src.nospace()) == '\0' )
          throw ParseError( "unexpected end of input" );

        if ( (closing = chr == '/') )
        {
          if ( (chr = src.nospace()) == '\0' )
            throw ParseError( "unexpected end of input" );
        }

      // get tag string
        for ( ; chr != '\0' && chr != '\n' && chr != '>'; chr = src.getnext() )
          str += chr;

      // check if tag ended not with '>'
        if ( chr != '>' )
          throw ParseError( "unexpected end of tag" );

        if ( trimString( str ).length() == 0 )
          throw ParseError( "empty tag name" );

      // if closing, rollback the tag history
        if ( closing )
        {
          while ( !tagStack.empty() && tagStack.back().key != str )
            tagStack.pop_back();

          if ( tagStack.empty() )
            throw ParseError( mtc::strprintf( "closing tag '%s' does not match any opening", str.c_str() ) );

          tagStack.pop_back();
        }
          else
        tagStack.push_back( { tagStack.back().tag->AddTextTag( str.c_str() ), str } );
      }
        else
      {
        do str += chr;
          while ( (chr = src.getnext()) != '\0' && chr != '\n' && chr != '<' );

        tagStack.back().tag->AddString( str );

        src.putback( chr );
      }
    }
  }

  void  Tags( mtc::api<IText> doc, std::function<char()> src )
  {
    FnStream                  stream( src );
    mtc::json::parse::reader  reader( stream );
    std::vector<Tag>          tagSet{ { doc, "" } };

    loadObject( tagSet, reader );
  }

}}}
