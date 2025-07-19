# include "../../src/indexer/static-entities.hpp"
# include "../../src/indexer/dynamic-entities.hpp"
# include <mtc/test-it-easy.hpp>
# include <mtc/byteBuffer.h>
# include <mtc/arena.hpp>
# include <mtc/zmap.h>
# include <thread>

using namespace DelphiX;
using namespace DelphiX::indexer;

auto  CreateEntityTable( std::initializer_list<std::pair<std::string, std::string>> entities ) -> std::vector<char>
{
  dynamic::EntityTable<>  dynamicEntities( 100, nullptr );

  for ( auto& next: entities )
    dynamicEntities.SetEntity( next.first, { next.second.data(), next.second.size() } );

  std::vector<char> serialBuffer( 0x100 );

  serialBuffer.resize( dynamicEntities.Serialize( serialBuffer.data() ) - serialBuffer.data() );

  return serialBuffer;
}

TestItEasy::RegisterFunc  static_entities( []()
  {
    TEST_CASE( "index/static-entities" )
    {
      auto  serialized = CreateEntityTable( {
        { "aaa", "metadata 1" },
        { "bbb", "metadata 2" },
        { "ccc", "metadata 3" } } );

      SECTION( "entities table may be created with default allocator" )
      {
        static_::EntityTable<>  entities( serialized );

        SECTION( "entities may be accessed by index" )
        {
          if ( REQUIRE_NOTHROW( entities.GetEntity( 0 ) ) )
            REQUIRE( entities.GetEntity( 0 ) == nullptr );
          if ( REQUIRE_NOTHROW( entities.GetEntity( 1 ) ) && REQUIRE( entities.GetEntity( 1 ) != nullptr ) )
          {
            REQUIRE( entities.GetEntity( 1 )->GetId() == "aaa" );
            REQUIRE( entities.GetEntity( 1 )->GetIndex() == 1 );
          }
          if ( REQUIRE_NOTHROW( entities.GetEntity( 3 ) ) && REQUIRE( entities.GetEntity( 3 ) != nullptr ) )
          {
            REQUIRE( entities.GetEntity( 3 )->GetId() == "ccc" );
            REQUIRE( entities.GetEntity( 3 )->GetIndex() == 3 );
          }
          if ( REQUIRE_NOTHROW( entities.GetEntity( 4 ) ) )
            REQUIRE( entities.GetEntity( 4 ) == nullptr );
        }
        SECTION( "entities may be accessed by id" )
        {
          REQUIRE_EXCEPTION( entities.GetEntity( "" ), std::invalid_argument );

          if ( REQUIRE_NOTHROW( entities.GetEntity( "aaa" ) ) && REQUIRE( entities.GetEntity( "aaa" ) != nullptr ) )
          {
            REQUIRE( entities.GetEntity( 1 )->GetId() == "aaa" );
            REQUIRE( entities.GetEntity( 1 )->GetIndex() == 1 );
          }
          if ( REQUIRE_NOTHROW( entities.GetEntity( "ccc" ) ) && REQUIRE( entities.GetEntity( "ccc" ) != nullptr ) )
          {
            REQUIRE( entities.GetEntity( 3 )->GetId() == "ccc" );
            REQUIRE( entities.GetEntity( 3 )->GetIndex() == 3 );
          }
          if ( REQUIRE_NOTHROW( entities.GetEntity( "q" ) ) )
            REQUIRE( entities.GetEntity( "q" ) == nullptr );
        }
        SECTION( "entity extras may be accessed" )
        {
          if ( REQUIRE_NOTHROW( entities.GetEntity( "aaa" ) ) && REQUIRE( entities.GetEntity( "aaa" ) != nullptr ) )
          {
            auto  extra = mtc::api<const mtc::IByteBuffer>();

            if ( REQUIRE_NOTHROW( extra = entities.GetEntity( "aaa" )->GetExtra() ) && REQUIRE( extra != nullptr ) )
            {
              REQUIRE( extra->GetLen() == 10 );
              REQUIRE( std::string_view( extra->GetPtr(), extra->GetLen() ) == "metadata 1" );
            }
          }
        }
      }
      SECTION( "entities table may be created with custom allocator also" )
      {
        mtc::Arena  memArena;
        auto        entities = memArena.Create<static_::EntityTable<mtc::Arena::allocator<char>>>(
          serialized, nullptr );

        SECTION( "iterators are available" )
        {
          SECTION( "* by index" )
          {
            if ( REQUIRE_NOTHROW( entities->GetIterator( 0U ) ) )
            {
              auto  it = entities->GetIterator( 0U );
              auto  pd = decltype(it.Curr())();

              if ( REQUIRE_NOTHROW( pd = it.Curr() ) )
                if ( REQUIRE( pd != nullptr ) )
                {
                  REQUIRE( pd == it.Curr() );
                  REQUIRE( pd->GetId() == "aaa" );
                }
              if ( REQUIRE_NOTHROW( pd = it.Next() ) )
                if ( REQUIRE( pd != nullptr ) )
                {
                  REQUIRE( pd == it.Curr() );
                  REQUIRE( pd->GetId() == "bbb" );
                }
              if ( REQUIRE_NOTHROW( pd = it.Next() ) )
                if ( REQUIRE( pd != nullptr ) )
                {
                  REQUIRE( pd == it.Curr() );
                  REQUIRE( pd->GetId() == "ccc" );
                }
              if ( REQUIRE_NOTHROW( pd = it.Next() ) )
                REQUIRE( pd == nullptr );
            }
          }
          SECTION( "* by id" )
          {
            auto  it = entities->GetIterator( "0" );
            auto  pd = decltype(it.Curr())();

            if ( REQUIRE_NOTHROW( pd = it.Curr() ) )
              if ( REQUIRE( pd != nullptr ) )
              {
                REQUIRE( pd == it.Curr() );
                REQUIRE( pd->GetId() == "aaa" );
              }
            if ( REQUIRE_NOTHROW( pd = it.Next() ) )
              if ( REQUIRE( pd != nullptr ) )
              {
                REQUIRE( pd == it.Curr() );
                REQUIRE( pd->GetId() == "bbb" );
              }
            if ( REQUIRE_NOTHROW( pd = it.Next() ) )
              if ( REQUIRE( pd != nullptr ) )
              {
                REQUIRE( pd == it.Curr() );
                REQUIRE( pd->GetId() == "ccc" );
              }
            if ( REQUIRE_NOTHROW( pd = it.Next() ) )
              REQUIRE( pd == nullptr );
          }
        }
      }
    }
  } );

