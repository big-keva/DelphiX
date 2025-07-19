# include "../../context/decomposer.hpp"
# include "../../context/make-image.hpp"
# include "../../context/index-keys.hpp"
# include "../../context/index-type.hpp"
# include "../../textAPI/DOM-text.hpp"
# include "../../textAPI/word-break.hpp"
# include <mtc/test-it-easy.hpp>

using namespace DelphiX;
using namespace DelphiX::context;
using namespace DelphiX::textAPI;

auto  StrKey( const char* str ) -> Key
{
  return { 0xff, codepages::mbcstowide( codepages::codepage_utf8, str ) };
}

TestItEasy::RegisterFunc  test_contents_processor( []()
{
  auto  text = Document();
    Document{
      { "title", {
          "Сказ про радугу",
          "/",
          "Rainbow tales" } },
      { "body", {
          "Как однажды Жак Звонарь"
          "Городской сломал фонарь" } },
      "Очень старый фонарь" }.CopyUtf16( &text );
  auto  body = BreakWords( text.GetBlocks() );
    CopyMarkup( body, text.GetMarkup() );

  TEST_CASE( "query/Decomposer" )
  {
    SECTION( "DefaultDecomposer works without any linguistics" )
    {
      Decomposer        decomposer;

      SECTION( "decomposer may be created" )
      {
        if ( REQUIRE_NOTHROW( decomposer = DefaultDecomposer() ) )
        {
          auto  contents = mtc::api<IImage>();

          SECTION( "decomposer fills images:" )
          {
            SECTION( "* as simple keys index")
            {
              if ( REQUIRE_NOTHROW( contents = Lite::Create() ) && REQUIRE( contents != nullptr ) )
              {
                REQUIRE_NOTHROW( decomposer( contents,
                  body.GetTokens(),
                  body.GetMarkup() ) );

                REQUIRE_NOTHROW( contents->List( [&]( const StrView&, const StrView& value, unsigned bkType )
                  {
                    REQUIRE( bkType == 0 );
                    REQUIRE( value.size() == 0 );
                  } ) );
              }
            }
            SECTION( "* as BM25 index")
            {
              if ( REQUIRE_NOTHROW( contents = BM25::Create() ) && REQUIRE( contents != nullptr ) )
              {
                REQUIRE_NOTHROW( decomposer( contents,
                  body.GetTokens(),
                  body.GetMarkup() ) );

                REQUIRE_NOTHROW( contents->List( [&]( const StrView& key, const StrView& value, unsigned bkType )
                  {
                    if ( REQUIRE( bkType == EntryBlockType::EntryCount ) && REQUIRE( value.size() != 0 ) )
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
            SECTION( "* as detailed index")
            {
              if ( REQUIRE_NOTHROW( contents = Rich::Create() ) && REQUIRE( contents != nullptr ) )
              {
                REQUIRE_NOTHROW( decomposer( contents,
                  body.GetTokens(),
                  body.GetMarkup() ) );

                REQUIRE_NOTHROW( contents->List( [&]( const StrView& key, const StrView& value, unsigned bkType )
                  {
                    if ( REQUIRE( bkType == EntryBlockType::EntryOrder ) && REQUIRE( value.size() != 0 ) )
                    {
                      int   getpos;
                      int   bkSize;
                      auto  source = ::FetchFrom( ::FetchFrom( value.data(),
                        bkSize ),
                        getpos );

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
      }
    }
  }
} );
