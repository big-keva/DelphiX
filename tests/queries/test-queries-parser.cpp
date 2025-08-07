# include "../../queries/parser.hpp"
# include <mtc/test-it-easy.hpp>

using namespace DelphiX;

TestItEasy::RegisterFunc  test_parser( []()
{
  TEST_CASE( "context/queries/parser" )
  {
    SECTION( "simple word produces simple word" )
    {
      REQUIRE( mtc::to_string( queries::ParseQuery( "a" ) ) == "a" );
    }
    SECTION( "query parser is stable to multiple braces" )
    {
      REQUIRE( queries::ParseQuery( "((abc))" ) == queries::ParseQuery( "abc" ) );

      SECTION( "... and causes exception on non-closed braces" )
      {  REQUIRE_EXCEPTION( queries::ParseQuery( "((abc)" ), queries::ParseError );  }
    }
    SECTION( "it breaks sequences to subsequences with operators" )
    {
      REQUIRE( mtc::to_string( queries::ParseQuery( "a && b & c" ) )
        == mtc::to_string( mtc::zmap{
          { "&&", mtc::array_zval{
            "a", "b", "c" } } } ) );

      SECTION( "operators priority is used" )
      {
        REQUIRE( mtc::to_string( queries::ParseQuery( "a & b | c !d" ) )
          == mtc::to_string( mtc::zmap{
            { "!", mtc::array_zval{
              mtc::zmap{ { "||", mtc::array_zval{
                mtc::zmap{ { "&&", mtc::array_zval{ "a", "b" } } },
                "c" } } },
            "d" } } } ) );
      }
    }
    SECTION( "sequences are treated as 'find 'near'est match'" )
    {
      REQUIRE( mtc::to_string( queries::ParseQuery( "a b c" ) ) == mtc::to_string( mtc::zmap{
        { "near", mtc::array_zval{ "a", "b", "c" } } } ) );
      REQUIRE( mtc::to_string( queries::ParseQuery( "a (b & c) d" ) ) == mtc::to_string( mtc::zmap{
        { "near", mtc::array_zval{
          "a",
          mtc::zmap{ { "&&", mtc::array_zval{ "b", "c" } } },
          "d" } } } ) );
    }
    SECTION( "quoted phrases are treated as phrases" )
    {
      SECTION( "single quote means exact sequence" )
      {
        REQUIRE( mtc::to_string( queries::ParseQuery( "'a b'" ) ) == mtc::to_string( mtc::zmap{
          { "sequence", mtc::array_zval{ "a", "b" } } } ) );
      }
      SECTION( "double quote means exact match" )
      {
        REQUIRE( mtc::to_string( queries::ParseQuery( "\"a b\"" ) ) == mtc::to_string( mtc::zmap{
          { "phrase", mtc::array_zval{ "a", "b" } } } ) );
      }
    }
    SECTION( "functions add limitations" )
    {
      SECTION( "functions have NAME(PARAMS) format" )
      {
        REQUIRE_EXCEPTION( queries::ParseQuery( "ctx(5, a" ), queries::ParseError );
      }
      SECTION( "context may be limited" )
      {
        REQUIRE( mtc::to_string( queries::ParseQuery( "ctx(5, a b c)" ) ) == mtc::to_string( mtc::zmap{
          { "limit", mtc::zmap{
            { "context", 5 },
            { "query", mtc::zmap{
              { "near", mtc::array_zval{ "a", "b", "c" } } } } } } } ) );
        REQUIRE( mtc::to_string( queries::ParseQuery( "ctx(5, a)" ) ) == mtc::to_string( mtc::zmap{
          { "limit", mtc::zmap{
            { "context", 5 },
            { "query", "a" } } } } ) );

        SECTION( "any value except unsigned integer causes exception" )
        {
          REQUIRE_EXCEPTION( queries::ParseQuery( "ctx(me)" ), queries::ParseError );
          REQUIRE_EXCEPTION( queries::ParseQuery( "ctx(5m)" ), queries::ParseError );
        }
        SECTION( "context limitation has to have comma and expression" )
        {
          REQUIRE_EXCEPTION( queries::ParseQuery( "ctx(5 a b c)" ), queries::ParseError );
        }
      }
      SECTION( "fields may be limited to value or set of values" )
      {
        REQUIRE( mtc::to_string( queries::ParseQuery( "cover(title, a b)" ) ) == mtc::to_string( mtc::zmap{
          { "cover", mtc::zmap{
            { "field", "title" },
            { "query", mtc::zmap{
              { "near", mtc::array_charstr{ "a", "b" } } } } } } } ) );
        REQUIRE( mtc::to_string( queries::ParseQuery( "match(title+body, a b)" ) ) == mtc::to_string( mtc::zmap{
          { "match", mtc::zmap{
            { "field", mtc::array_charstr{ "body", "title" } },
            { "query", mtc::zmap{
              { "near", mtc::array_charstr{ "a", "b" } } } } } } } ) );
      }
    }
  }
} );
