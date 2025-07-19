# include "../../context/index-keys.hpp"
# include <mtc/test-it-easy.hpp>
# include <moonycode/codes.h>

using namespace DelphiX;
using namespace DelphiX::context;

TestItEasy::RegisterFunc  test_key( []()
{
  TEST_CASE( "query/Key" )
  {
    Key       key;
    widechar  str[10];

    SECTION( "uninitialized Key is empty" )
    {
      REQUIRE( key.has_int() == false );
      REQUIRE( key.has_str() == false );
      REQUIRE( key.has_cls() == false );

      REQUIRE( key.get_idl() == unsigned(-1) );
      REQUIRE( key.get_int() == 0 );
      REQUIRE( key.get_cls() == 0 );
      REQUIRE( key.get_len() == 0 );
      REQUIRE_EXCEPTION( key.get_str( nullptr, 0 ), std::invalid_argument );
    }
    SECTION( "Key may be initialized" )
    {
      SECTION( "- as int" )
      {
        REQUIRE_NOTHROW( (key = { 0, 5U }) );

        REQUIRE( key.has_int() );
        REQUIRE( key.has_str() == false );
        REQUIRE( key.has_cls() == false );

        REQUIRE( key.get_idl() == 0 );
        REQUIRE( key.get_int() == 5 );

        REQUIRE( key.get_cls() == 0 );
        REQUIRE( key.get_len() == 0 );
        REQUIRE_EXCEPTION( key.get_str( nullptr, 0 ), std::invalid_argument );
      }
      SECTION( "- as str" )
      {
        REQUIRE_NOTHROW( (key = { 1, codepages::mbcstowide( codepages::codepage_utf8, "слово" ) }) );

        REQUIRE( key.has_int() == false );
        REQUIRE( key.has_str() );
        REQUIRE( key.has_cls() == false );

        REQUIRE( key.get_idl() == 1 );
        REQUIRE( key.get_int() == 0 );

        REQUIRE( key.get_cls() == 0 );
        REQUIRE( key.get_len() == 5 );

        REQUIRE_EXCEPTION( key.get_str( nullptr, 0 ), std::invalid_argument );

        REQUIRE_NOTHROW( key.get_str( str, 10 ) );
        REQUIRE( codepages::mbcstowide( codepages::codepage_utf8, "слово" ) == str );
      }
      SECTION( "- as cls" )
      {
        REQUIRE_NOTHROW( (key = { 1160, 9060, codepages::mbcstowide( codepages::codepage_utf8, "дело" ) }) );

        REQUIRE( key.has_int() == false );
        REQUIRE( key.has_str() );
        REQUIRE( key.has_cls() );

        REQUIRE( key.get_idl() == 1160 );
        REQUIRE( key.get_int() == 0 );

        REQUIRE( key.get_cls() == 9060 );
        REQUIRE( key.get_len() == 4 );

        REQUIRE_EXCEPTION( key.get_str( nullptr, 0 ), std::invalid_argument );

        REQUIRE_NOTHROW( key.get_str( str, 10 ) );
        REQUIRE( codepages::mbcstowide( codepages::codepage_utf8, "дело" ) == str );
      }
    }
    SECTION( "long keys are allocated in heap" )
    {
      auto      str = "это очень длинный ключ, сделанный из строки utf8 заведомо большой длины";
      widechar  buf[100];

      key = Key( 96, 11627, codepages::mbcstowide( codepages::codepage_utf8, str ) );

      REQUIRE( key.has_int() == false );
      REQUIRE( key.has_str() == true );
      REQUIRE( key.has_cls() == true );
      REQUIRE( key.get_idl() == 96 );
      REQUIRE( key.get_cls() == 11627 );
      REQUIRE( key.get_len() == 71 );

      REQUIRE( key.get_str( buf, 100 ) != nullptr );
      REQUIRE( codepages::mbcstowide( codepages::codepage_utf8, str ) == buf );
    }
  }
} );
