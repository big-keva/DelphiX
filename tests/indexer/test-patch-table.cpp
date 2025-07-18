# include "src/indexer/patch-table.hpp"
# include <mtc/test-it-easy.hpp>
# include <mtc/arena.hpp>
# include <thread>

using namespace DelphiX;
using namespace DelphiX::indexer;

TestItEasy::RegisterFunc  patch_table( []()
  {
    TEST_CASE( "index/patch-table" )
    {
      SECTION( "patch table may be created with default allocator" )
      {
        PatchTable patch_table( 301 );

        SECTION( "patch records may be added and accessed by two type of keys:" )
        {
          SECTION( "* integer keys" )
          {
            REQUIRE_NOTHROW( patch_table.Delete( "111", 111 ) );
            REQUIRE_NOTHROW( patch_table.Delete( "333", 333 ) );
            REQUIRE_NOTHROW( patch_table.Delete( "777", 777 ) );
            REQUIRE_NOTHROW( patch_table.Delete( "999", 999 ) );

            if ( REQUIRE( patch_table.Search( 111 ) != nullptr ) )
              REQUIRE( patch_table.Search( 111 )->GetLen() == size_t(-1) );

            if ( REQUIRE( patch_table.Search( 333 ) != nullptr ) )
              REQUIRE( patch_table.Search( 333 )->GetLen() == size_t(-1) );

            if ( REQUIRE( patch_table.Search( 777 ) != nullptr ) )
              REQUIRE( patch_table.Search( 777 )->GetLen() == size_t(-1) );

            if ( REQUIRE( patch_table.Search( 999 ) != nullptr ) )
              REQUIRE( patch_table.Search( 999 )->GetLen() == size_t(-1) );

            REQUIRE( patch_table.Search( 222 ) == nullptr );
          }
          SECTION( "* string_view keys" )
          {
            REQUIRE_NOTHROW( patch_table.Delete( "aaa", 0xa ) );

            if ( REQUIRE_NOTHROW( patch_table.Search( "aaa" ) ) )
              if ( REQUIRE( patch_table.Search( "aaa" ) != nullptr ) )
                REQUIRE( patch_table.Search( "aaa" )->GetLen() == size_t(-1) );

            if ( REQUIRE_NOTHROW( patch_table.Search( "bbb" ) ) )
              REQUIRE( patch_table.Search( "bbb" ) == nullptr );
          }
        }
        SECTION( "Update() patch records allocate patch data" )
        {
          if ( REQUIRE_NOTHROW( patch_table.Update( "ccc", 0, { "abc" } ) ) )
            if ( REQUIRE( patch_table.Search( 0 ) != nullptr ) )
              REQUIRE( patch_table.Search( 0 )->GetLen() == 3 );

          SECTION( "it overrides old patch data" )
          {
            if ( REQUIRE_NOTHROW( patch_table.Update( "ccc", 0, { "def" } ) ) )
              if ( REQUIRE( patch_table.Search( 0 ) != nullptr ) )
                if ( REQUIRE( patch_table.Search( 0 )->GetPtr() != nullptr ) )
                {
                  REQUIRE( patch_table.Search( 0 )->GetLen() == 3 );
                  REQUIRE( std::string_view( patch_table.Search( 0 )->GetPtr(), 3 ) == "def" );
                }
          }
          SECTION( "and does not override Delete() patches" )
          {
            if ( REQUIRE_NOTHROW( patch_table.Update( "111", 111, { "abc" } ) ) )
              if ( REQUIRE( patch_table.Search( 111 ) != nullptr ) )
                REQUIRE( patch_table.Search( 111 )->GetLen() == size_t(-1) );
          }
        }
        SECTION( "Delete() patch records override patch data" )
        {
          if ( REQUIRE_NOTHROW( patch_table.Delete( "ccc", 0 ) ) )
            if ( REQUIRE( patch_table.Search( 0 ) != nullptr ) )
              REQUIRE( patch_table.Search( 0 )->GetLen() == size_t(-1) );
        }
      }
      SECTION( "PatchTable may be created in Arena" )
      {
        auto  arena = mtc::Arena();
        auto  patch = arena.Create<PatchTable<mtc::Arena::allocator<char>>>( 303 );

        REQUIRE_NOTHROW( patch->Delete( "aaa", 0 ) );
        if ( REQUIRE_NOTHROW( patch->Search( 0 ) ) )
          if ( REQUIRE( patch->Search( 0 ) != nullptr ) )
            REQUIRE( patch->Search( 0 )->GetLen() == size_t(-1) );

        REQUIRE_NOTHROW( patch->Update( "bbb", 1, "updated" ) );

        REQUIRE_NOTHROW( patch->Search( "bbb" ) );

        if ( REQUIRE( patch->Search( "bbb" ) != nullptr ) )
        {
          REQUIRE( patch->Search( "bbb" )->GetPtr() != nullptr );
          REQUIRE( patch->Search( "bbb" )->GetLen() == 7 );
          REQUIRE( std::string_view( patch->Search( "bbb" )->GetPtr(), 7 ) == "updated" );
        }
      }
    }
  } );

