# include "textAPI/DOM-dump.hpp"
# include <moonycode/codes.h>

// tags implementation

namespace DelphiX {
namespace textAPI {
namespace dump_as {

  using FnSink = std::function<void(const char*, size_t)>;

  class JsonTag final: public IText
  {
    FnSink      fWrite;
    unsigned    uShift;
    size_t      nItems = 0;
    bool        is_Tag;

    implement_lifetime_control

  public:
    JsonTag( FnSink f, unsigned u, const std::string& t = {} ):
      fWrite( f ),
      uShift( u ),
      is_Tag( !t.empty() )
    {
      if ( is_Tag )
      {
        for ( unsigned u = 0; u != uShift; ++u )
          fWrite( "  ", 2 );

        fWrite( "{ \"", 3 );
          Print( t.c_str(), t.length() );
        fWrite( "\": ", 3 );
      }
      fWrite( "[\n", 1 );
    }
    ~JsonTag()
    {
      if ( nItems != 0 )
        fWrite( "\n", 1 );

      for ( unsigned u = 0; u != uShift; ++u )
        fWrite( "  ", 2 );

      fWrite( "]", 1 );

      if ( is_Tag )
        fWrite( " }", 2 );
    }
    auto  AddTextTag( const char* tag, size_t len ) -> mtc::api<IText> override
    {
      if ( len == (size_t)-1 )
        for ( len = 0; tag[len] != 0; ++len ) (void)NULL;

      if ( nItems != 0 )  fWrite( ",\n", 2 );
        else fWrite( "\n", 1 );

      return new JsonTag( fWrite, uShift + 1, std::string( tag, len ) );
    }
    void  AddCharStr( const char* str, size_t len, unsigned codepage ) override
    {
      auto  putstr = std::string();

      if ( len == (size_t)-1 )
        for ( len = 0; str[len] != 0; ++len ) (void)NULL;

      if ( nItems != 0 )  fWrite( ",\n", 2 );
        else fWrite( "\n", 1 );

      for ( unsigned u = 0; u != uShift + 1; ++u )
        fWrite( "  ", 2 );

      if ( codepage != codepages::codepage_utf8 )
        putstr = codepages::mbcstombcs( codepages::codepage_utf8, codepage, str, len );
      else putstr = std::string( str, len );

      fWrite( "\"", 1 );
        Print( putstr.c_str(), putstr.length() );
      fWrite( "\"", 1 );

      ++nItems;
    }
    void  AddWideStr( const widechar* wcs, size_t len ) override
    {
      AddString( codepages::widetombcs( codepages::codepage_utf8, wcs, len ).c_str(),
        codepages::codepage_utf8 );
    }
  protected:
    void  Print( const char* str, size_t len ) const
    {
      static const char escapeChar[] = "\"\\/\b\f\n\r\t";

      auto end = str + len;

      while ( str != end )
      {
        auto  org = str;

        while ( str != end && strchr( escapeChar, *str ) == nullptr )
          ++str;

        if ( str != org ) fWrite( org, str - org );
          else
        switch ( *str++ )
        {
          case '\"':  fWrite( "\\\"", 2 );  break;
          case '\\':  fWrite( "\\\\", 2 );  break;
          case '/':   fWrite( "\\/",  2 );  break;
          case '\b':  fWrite( "\\b", 2 );  break;
          case '\f':  fWrite( "\\f", 2 );  break;
          case '\n':  fWrite( "\\n", 2 );  break;
          case '\r':  fWrite( "\\r", 2 );  break;
          case '\t':  fWrite( "\\t", 2 );  break;
          default: throw std::logic_error( "invalid escape sequence" );
        }
      }
    }
  };

  auto  Json( std::function<void( const char*, size_t )> fn ) -> mtc::api<IText>
  {
    return new JsonTag( fn, 0 );
  }

}}}
