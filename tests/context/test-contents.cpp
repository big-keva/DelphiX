# include "../../context/x-contents.hpp"
# include "../../context/processor.hpp"
# include "../../context/fields-man.hpp"
# include <mtc/test-it-easy.hpp>

using namespace DelphiX;
using namespace DelphiX::context;

auto  StrKey( const char* str ) -> Key
{
  return { 0xff, codepages::mbcstowide( codepages::codepage_utf8, str ) };
}

TestItEasy::RegisterFunc  test_contents_processor( []()
{
  auto  proc = Processor();
  auto  text = DeliriX::Text();

  CopyUtf16( &text, DeliriX::Text{
    { "title", {
        "Сказ про радугу",
        "/",
        "Rainbow tales" } },
    { "body", {
        "Как однажды Жак Звонарь"
        "Городской сломал фонарь" } },
    "Очень старый фонарь" } );
  auto  body =
    proc.WordBreak( text );
    proc.SetMarkup( body, text );
    proc.Lemmatize( body );
  auto  contents = mtc::api<IContents>();

  TEST_CASE( "context/contents" )
  {
    SECTION( "contents may be created" )
    {
      SECTION( "* as 'mini'" )
      {
        FieldManager  fieldMan;

        if ( REQUIRE_NOTHROW( contents = GetMiniContents( body.GetLemmas(), body.GetMarkup(), fieldMan ) ) )
          if ( REQUIRE( contents != nullptr ) )
          {
            REQUIRE_NOTHROW( contents->List( [&]( const std::string_view&, const std::string_view& value, unsigned bkType )
              {
                REQUIRE( bkType == 0 );
                REQUIRE( value.size() == 0 );
              } ) );
          }
      }
      SECTION( "* as 'BM25'" )
      {
        FieldManager  fieldMan;

        if ( REQUIRE_NOTHROW( contents = GetBM25Contents( body.GetLemmas(), body.GetMarkup(), fieldMan ) ) )
          if ( REQUIRE( contents != nullptr ) )
          {
            REQUIRE_NOTHROW( contents->List( [&]( const std::string_view& key, const std::string_view& value, unsigned bkType )
              {
                if ( REQUIRE( bkType == 10 ) && REQUIRE( value.size() != 0 ) )
                {
                  auto  curkey = Key( key.data(), key.size() );
                  int   ncount;

                  ::FetchFrom( value.data(), ncount );

                  if ( curkey == StrKey( "фонарь" ) ) REQUIRE( ncount == 2 );
                    else  REQUIRE( ncount == 1 );
                }
              } ) );
          }
      }
      SECTION( "* as 'Rich'" )
      {
        FieldManager  fieldMan;

        if ( REQUIRE_NOTHROW( contents = GetRichContents( body.GetLemmas(), body.GetMarkup(), fieldMan ) ) )
          if ( REQUIRE( contents != nullptr ) )
          {
            REQUIRE_NOTHROW( contents->List( [&]( const std::string_view& key, const std::string_view& value, unsigned bkType )
              {
                if ( bkType == 99 )
                  return;

                if ( REQUIRE( bkType == 20 ) && REQUIRE( value.size() != 0 ) )
                {
                  int   getpos;
                  auto  source = ::FetchFrom( value.data(), getpos );

                  if ( Key( key.data(), key.size() ) == StrKey( "радугу" ) )
                  {
                    REQUIRE( getpos == 2 );
                  }
                    else
                  if ( Key( key.data(), key.size() ) == StrKey( "фонарь" ) )
                  {
                    REQUIRE( getpos == 11 );
                      ::FetchFrom( source, getpos );
                    REQUIRE( getpos == 2 );
                  }
                }
              } ) );
          }
      }
    }
  }
} );
