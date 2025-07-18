# include "../../textAPI/DOM-dump.hpp"
# include <moonycode/codes.h>

namespace DelphiX {
namespace textAPI {
namespace dump_as {

  using FnSink = std::function<void(const char*, size_t)>;

  class TagsTag final: public IText
  {
    FnSink          fWrite;
    const unsigned  encode = codepages::codepage_utf8;
    unsigned        uShift;
    std::string     tagStr;

    implement_lifetime_control

  public:
    TagsTag( FnSink f, unsigned c, unsigned u, std::string&& t = {} ):
      fWrite( f ),
      encode( c ),
      uShift( u ),
      tagStr( std::move( t ) )
    {
      if ( !tagStr.empty() )
      {
        for ( unsigned u = 0; u != uShift; ++u )
          fWrite( "  ", 2 );

        fWrite( "<", 1 );
          Print( tagStr.c_str(), tagStr.length() );
        fWrite( ">\n", 2 );
      }
    }
    ~TagsTag()
    {
      if ( !tagStr.empty() )
      {
        for ( unsigned u = 0; u != uShift; ++u )
          fWrite( "  ", 2 );

        fWrite( "</", 2 );
          Print( tagStr.c_str(), tagStr.length() );
        fWrite( ">\n", 2 );
      }
    }
    auto  AddTextTag( const char* tag, size_t len ) -> mtc::api<IText> override
    {
      if ( len == size_t(-1) )
        for ( len = 0; tag != nullptr && tag[len] != 0; ++len )
          (void)NULL;

      return new TagsTag( fWrite, encode, uShift + 1, std::string( tag, len ) );
    }
    void  AddCharStr( const char* str, size_t len, unsigned codepage ) override
    {
      std::string  sprint;

      if ( len == (size_t)-1 )
        for ( len = 0; str[len] != 0; ++len ) (void)NULL;

      if ( encode != unsigned(-1) && codepage != encode )
      {
        sprint = codepages::mbcstombcs( encode, codepage, str, len );
        str = sprint.c_str();
        len = sprint.size();
      }

      if ( !tagStr.empty() )
      {
        for ( unsigned u = 0; u <= uShift; ++u )
          fWrite( "  ", 2 );
      }

      Print( str, len );
      fWrite( "\n", 1 );
    }
    void  AddWideStr( const widechar* wcs, size_t len ) override
    {
      AddString( codepages::widetombcs( encode, wcs, len ), encode );
    }
  protected:
    void  Print( const char* str, size_t len ) const
    {
      auto end = str + len;

      while ( str != end )
      {
        auto org = str;

        while ( str != end && *str != '<' && *str != '>' && *str != '&' )
          ++str;

        if ( str != org ) fWrite( org, str - org );
          else
        switch ( *str++ )
        {
          case '<': fWrite( "&lt;", 4 );  break;
          case '>': fWrite( "&gt;", 4 );  break;
          default:  fWrite( "&amp;", 5 );
        }
      }
    }
  };

  auto  Tags( std::function<void( const char*, size_t )> fn, unsigned cp ) -> mtc::api<IText>
  {
    return new TagsTag( fn, cp, 0 );
  }

}}}
