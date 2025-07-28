#include <context/lemmatizer.hpp>
#include <textAPI/DOM-text.hpp>

# include "mtc/test-it-easy.hpp"
# include "../../../lang-api.hpp"

using namespace DelphiX;

# define __Q__(x) #x
# define QUOTE(x) __Q__(x)

class Stemka: public ILemmatizer
{
  implement_lifetime_control

public:
  int   Lemmatize( IWord*, const widechar*, size_t ) override;

};

TestItEasy::RegisterFunc  test_stemka( []()
  {
    TEST_CASE( "addon/stemka" )
    {
      auto  text = textAPI::Document();
        textAPI::Document{
          "Сказка о рыбаке и рыбке",
          "Жили-были старик и SpaceX"
        }.CopyUtf16( &text );
      /*
      auto  body = textAPI::BreakWords( text.GetBlocks() );
      */
      SECTION( "stemka-api library may be loaded" )
      {
        auto  stemka = mtc::api<ILemmatizer>();

        if ( !REQUIRE_NOTHROW( stemka = context::LoadLemmatizer( QUOTE(STEMKA_SO_PATH), "" ) ) )
          break;
        if ( !REQUIRE( stemka != nullptr ) )
          break;

        SECTION( "called for text, it creates fuzzy keys for cyrilllic words" )
        {
          /*
          REQUIRE_NOTHROW( decomposer( &collector,
            body.GetTokens(),
            body.GetMarkup() ) );

          for ( int i = 0; i != collector.size(); ++i )
          {
            fprintf( stdout, "[%u]\n", i );

            for ( auto& skey: collector[i] )
            {
              widechar  keystr[0x100];

              if ( skey.has_cls() )
              {
                fprintf( stdout, "  %u\tcls:%u str:%s\n", skey.get_idl(), skey.get_cls(),
                  codepages::widetombcs( codepages::codepage_utf8, skey.get_str( keystr, 0x100 ) ).c_str() );
              }
                else
              {
                fprintf( stdout, "  %u\tstr:%s\n", skey.get_idl(),
                  codepages::widetombcs( codepages::codepage_utf8, skey.get_str( keystr, 0x100 ) ).c_str() );
              }
            }
          }
          SECTION( "keys are created for all the words" )
          {
            REQUIRE( collector.size() == 11 );
          }
          SECTION( "keys for long cyrillic words are fuzzy" )
          {
            if ( REQUIRE( collector[0].size() != 0 ) )
              REQUIRE( collector[0].front().get_cls() == 1 );
          }
          */
        }
      }
    }
  } );
