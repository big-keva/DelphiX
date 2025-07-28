# include "../../context/fields-man.hpp"
# include <mtc/test-it-easy.hpp>
# include <moonycode/codes.h>
#include <mtc/json.h>

using namespace DelphiX;
using namespace DelphiX::context;

TestItEasy::RegisterFunc  test_fields_manager( []()
{
  TEST_CASE( "context/FieldManager" )
  {
    SECTION( "FieldManager may be created with API" )
    {
      FieldManager  fm;

      SECTION( "fields may be added" )
      {
        FieldOptions* options;

        if ( REQUIRE_NOTHROW( options = fm.Add( "aaa" ) ) && REQUIRE( options != nullptr ) )
        {
          REQUIRE( options->id == 0 );
          REQUIRE( options->name == "aaa" );
          REQUIRE( fabs(options->weight - 1.0) < 1e-7 );
          REQUIRE( options->options == 0 );
          REQUIRE( options->indents.lower.min == 2 );
          REQUIRE( options->indents.lower.max == 8 );
          REQUIRE( options->indents.upper.min == 2 );
          REQUIRE( options->indents.upper.max == 8 );
        }
        if ( REQUIRE_NOTHROW( options = fm.Add( "bbb" ) ) && REQUIRE( options != nullptr ) )
        {
          REQUIRE( options->id == 1 );
          REQUIRE( options->name == "bbb" );
        }
      }
      SECTION( "fields may be accessed" )
      {
        const FieldOptions* options;

        SECTION( "* by name" )
        {
          if ( REQUIRE_NOTHROW( options = fm.Get( "aaa" ) ) && REQUIRE( options != nullptr ) )
            REQUIRE( options->id == 0 );
        }
        SECTION( "* by id" )
        {
          if ( REQUIRE_NOTHROW( options = fm.Get( 0 ) ) && REQUIRE( options != nullptr ) )
            REQUIRE( options->name == "aaa" );
        }
      }
    }
    SECTION( "FieldManager may be created from parsed json" )
    {
      FieldManager  fm;

      SECTION( "with empty source, it returns empty fields list" )
      {
        if ( REQUIRE_NOTHROW( fm = LoadFields( mtc::zmap{}, "fields" ) ) )
          REQUIRE( fm.Get( 0 ) == nullptr );
      }
      SECTION( "with invalid source, it throws invalid_argument" )
      {
        REQUIRE_EXCEPTION( LoadFields( mtc::zmap{
          { "fields", "as string" } }, "fields" ), std::invalid_argument );
      }
      SECTION( "with empty array, it returns empty manager" )
      {
        REQUIRE_NOTHROW( LoadFields( {} ) );
        REQUIRE_NOTHROW( LoadFields( mtc::zmap{ { "fields", mtc::array_zmap{} } }, "fields" ) );
      }
      SECTION( "with invalid array type, it throws invalid_argument" )
      {
        REQUIRE_EXCEPTION( LoadFields( mtc::zmap{ { "fields", mtc::array_zval{
          "string key", 5 } } }, "fields" ), std::invalid_argument );
      }
      SECTION( "empty field descriptions are not allowed" )
      {
        REQUIRE_EXCEPTION( fm = LoadFields( mtc::zmap{ { "fields", mtc::array_zmap{
          {}, {} } } }, "fields" ), std::invalid_argument );
      }
      SECTION( "fields are loaded from config one by another" )
      {
        if ( REQUIRE_NOTHROW( fm = LoadFields( mtc::zmap{ { "fields", mtc::array_zmap{
          { { "name", "title" } },
          { { "name", "body" } } } } }, "fields" ) ) )
        {
          REQUIRE( fm.Get( 0 )->name == "title" );
            REQUIRE( fm.Get( 0 )->id == 0 );
          REQUIRE( fm.Get( 1 )->name == "body" );
            REQUIRE( fm.Get( 1 )->id == 1 );
        };
      }
      SECTION( "field quotation indents may provided" )
      {
        SECTION( "non-structure 'indents' cause invalid_argument" )
        {
          REQUIRE_EXCEPTION( fm = LoadFields( mtc::array_zmap{
          { { "name", "body" },
            { "indents", 5 } } } ), std::invalid_argument );
        }
        SECTION( "'indents' may contain only 'lower' and 'upper' as structs" )
        {
          REQUIRE_EXCEPTION( fm = LoadFields( mtc::array_zmap{
          { { "name", "body" },
            { "indents", mtc::zmap{
                { "lower", 5 } } } } } ), std::invalid_argument );
          REQUIRE_EXCEPTION( fm = LoadFields( mtc::array_zmap{
          { { "name", "body" },
            { "indents", mtc::zmap{
                { "medium", mtc::zmap() } } } } } ), std::invalid_argument );

          SECTION( "'indents':'lower' and 'upper may contain only 'min' and 'max'" )
          {
            REQUIRE_EXCEPTION( fm = LoadFields( mtc::array_zmap{
            { { "name", "body" },
              { "indents", mtc::zmap{
                  { "lower", mtc::zmap{
                    { "all", 5 } } } } } } } ), std::invalid_argument );
          }
          SECTION( "valid 'indents' fill indentation structure" )
          {
            if ( REQUIRE_NOTHROW( fm = LoadFields( mtc::array_zmap{
              { { "name", "body" },
                { "indents", mtc::zmap{
                    { "lower", mtc::zmap{
                      { "min", 3 },
                      { "max", 9 } } },
                    { "upper", mtc::zmap{
                      { "min", 5 } } } } } } } ) ) )
            {
              REQUIRE( fm.Get( 0 )->indents.lower.min == 3 );
              REQUIRE( fm.Get( 0 )->indents.lower.max == 9 );
              REQUIRE( fm.Get( 0 )->indents.upper.min == 5 );
              REQUIRE( fm.Get( 0 )->indents.upper.max == 8 );
            }
          }
        }
        SECTION( "'options' may be specified as css-style string" )
        {
          SECTION( "'options' may be only string" )
          {
            REQUIRE_EXCEPTION( fm = LoadFields( mtc::array_zmap{
              { { "name", "body" },
                { "options", 5 } } } ), std::invalid_argument );
          }
          SECTION( "'options' may contain only 'word-break' and 'contents' fields" )
          {
            REQUIRE_EXCEPTION( fm = LoadFields( mtc::array_zmap{
              { { "name", "body" },
                { "options", "my-code: true" } } } ), std::invalid_argument );
            REQUIRE_NOTHROW( fm = LoadFields( mtc::array_zmap{
              { { "name", "body" },
                { "options", "word-break: true; contents: text" } } } ) );
          }
          SECTION( "'options' affect flags" )
          {
            REQUIRE_NOTHROW( fm = LoadFields( mtc::array_zmap{
              { { "name", "body" },
                { "options", "word-break:false;" } } } ) );
            REQUIRE( (fm.Get( 0 )->options & FieldOptions::ofNoBreakWords) != 0 );
          }
        }
      }
    }
  }
} );
