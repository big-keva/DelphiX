# include "src/indexer/dynamic-entities.hpp"
# include <mtc/test-it-easy.hpp>
# include <mtc/arena.hpp>
# include <thread>

# define ENTITY_ID_PREFIX "entity_with_long_id_is_defined_to_force_std::string_to_allocate_memory_"

using namespace DelphiX;
using namespace DelphiX::indexer;

TestItEasy::RegisterFunc  dynamic_entities( []()
  {
    TEST_CASE( "index/dynamic-entities" )
    {
      SECTION( "entities table may be created with default allocator" )
      {
        const size_t max_document_count = 10;

        using EntityTable = dynamic::EntityTable<>;

        auto  entity_table = std::unique_ptr<EntityTable>( nullptr );

        REQUIRE_NOTHROW( entity_table = std::make_unique<EntityTable>( max_document_count, nullptr ) );
          REQUIRE( entity_table->GetMaxEntities() == max_document_count );
          REQUIRE( entity_table->GetHashTableSize() == UpperPrime( max_document_count ) );
        SECTION( "for invalid access index, GetEntity( ... ) throws std::invalid_argument" )
        {
          REQUIRE_EXCEPTION( entity_table->GetEntity( 0 ), std::invalid_argument );
          REQUIRE_EXCEPTION( entity_table->GetEntity( uint32_t(-1) ), std::invalid_argument );
          REQUIRE_EXCEPTION( entity_table->GetEntity( max_document_count ), std::invalid_argument );
        }
        SECTION( "with empty table, GetEntity( ... ) always return nullptr" )
        {
          REQUIRE_NOTHROW( entity_table->GetEntity( 1 ) );
          REQUIRE_NOTHROW( entity_table->GetEntity( 1 ) == nullptr );
        }
        SECTION( "entities may be set to the table" )
        {
          auto      entity = mtc::api<EntityTable::Entity>();
          uint32_t  deldoc;

          SECTION( "without id, it throws std::invalid_argument" )
          {
            REQUIRE_EXCEPTION( entity_table->SetEntity( "" ), std::invalid_argument );
          }
          SECTION( "with id, it creates a new document record" )
          {
            REQUIRE_NOTHROW( entity = entity_table->SetEntity( ENTITY_ID_PREFIX "aaa", {}, &deldoc ) );
              REQUIRE( entity != nullptr );
              REQUIRE( entity->GetId() == ENTITY_ID_PREFIX "aaa" );
              REQUIRE( entity->GetIndex() == 1 );
              REQUIRE( deldoc == uint32_t(-1) );
          }
          SECTION( "if overriden, it removes existing document and stores new version" )
          {
            REQUIRE_NOTHROW( entity = entity_table->SetEntity( ENTITY_ID_PREFIX "aaa", {}, &deldoc ) );
              REQUIRE( entity != nullptr );
              REQUIRE( entity->GetId() == ENTITY_ID_PREFIX "aaa" );
              REQUIRE( entity->GetIndex() == 2 );
              REQUIRE( deldoc == 1 );
          }
          SECTION( "up to max_count of documents may be set" )
          {
            for ( auto i = 2; i != max_document_count - 1; ++i )
            {
              auto  entkey = mtc::strprintf( ENTITY_ID_PREFIX "%c%c%c", 'a' + i, 'a' + i, 'a' + i );

              REQUIRE_NOTHROW( entity = entity_table->SetEntity( entkey, {}, &deldoc ) );
                REQUIRE( entity != nullptr );
                REQUIRE( entity->GetId() == entkey );
                REQUIRE( entity->GetIndex() == 1 + i );
                REQUIRE( deldoc == uint32_t(-1) );
            }
            REQUIRE( entity_table->GetEntityCount() == max_document_count - 1 );
          }
          SECTION( "after filling entity table, any SetEntity() causes overflow" )
          {
            REQUIRE_EXCEPTION( entity = entity_table->SetEntity( "qqq" ), index_overflow );
          }
          SECTION( "entity may be removed" )
          {
            REQUIRE( entity_table->DelEntity( ENTITY_ID_PREFIX "ccc" ) != uint32_t(-1) );
            REQUIRE( entity_table->DelEntity( "qqq" ) == uint32_t(-1) );
          }
          SECTION( "entities may be accessed by index and by id" )
          {
            SECTION( "by index" )
            {
              REQUIRE_EXCEPTION( entity_table->GetEntity( 0U ),
                std::invalid_argument );
              REQUIRE_EXCEPTION( entity_table->GetEntity( -1 ),
                std::invalid_argument );
              REQUIRE_EXCEPTION( entity_table->GetEntity( 100 ),
                std::invalid_argument );
              if ( REQUIRE_NOTHROW( entity = entity_table->GetEntity( 1U ) ) )
                REQUIRE( entity == nullptr );
              if ( REQUIRE_NOTHROW( entity = entity_table->GetEntity( 2U ) ) )
                REQUIRE( entity != nullptr );
            }
            SECTION( "by id" )
            {
              if ( REQUIRE_NOTHROW( entity = entity_table->GetEntity( "xxx" ) ) )
                REQUIRE( entity == nullptr );
              if ( REQUIRE_NOTHROW( entity = entity_table->GetEntity( ENTITY_ID_PREFIX "aaa" ) ) )
              {
                REQUIRE( entity != nullptr );
                REQUIRE( entity->GetId() == ENTITY_ID_PREFIX "aaa" );
              }
              if ( REQUIRE_NOTHROW( entity = entity_table->GetEntity( ENTITY_ID_PREFIX "ccc" ) ) )
                REQUIRE( entity == nullptr );
            }
          }
          SECTION( "entity table is iterable" )
          {
            EntityTable::Iterator               iterator;
            mtc::api<const EntityTable::Entity> element;

            SECTION( "it is iterable by index:" )
            {
              SECTION( "At start iterator is positioned to first entity," )
              {
                if ( REQUIRE_NOTHROW( iterator = entity_table->GetIterator( 0U ) ) )
                  if ( REQUIRE_NOTHROW( element = iterator.Curr() ) )
                  {
                    REQUIRE( element != nullptr );
                    REQUIRE( element->GetIndex() == 2 );
                  }
              }
              SECTION( "but it may be positioned to some other index;" )
              {
                if ( REQUIRE_NOTHROW( iterator = entity_table->GetIterator( 8U ) ) )
                  if ( REQUIRE_NOTHROW( element = iterator.Curr() ) )
                  {
                    REQUIRE( element != nullptr );
                    REQUIRE( element->GetIndex() == 8 );
                  }
              }
              SECTION( "Next() moves it to the next entity" )
              {
                if ( REQUIRE_NOTHROW( element = iterator.Next() ) )
                  if ( REQUIRE( element != nullptr ) )
                    REQUIRE( element->GetIndex() == 9 );
              }
              SECTION( "until the end of iterable list." )
              {
                REQUIRE_NOTHROW( element = iterator.Next() );
                REQUIRE( element == nullptr );
              }
            }
            SECTION( "and it is iterable by id:" )
            {
              SECTION( "At start iterator is positioned to first entity," )
              {
                if ( REQUIRE_NOTHROW( iterator = entity_table->GetIterator( "" ) ) )
                  if ( REQUIRE_NOTHROW( element = iterator.Curr() ) )
                  {
                    REQUIRE( element != nullptr );
                    REQUIRE( element->GetId() == ENTITY_ID_PREFIX "aaa" );
                  }
              }
              SECTION( "but it may be positioned to some other index;" )
              {
                if ( REQUIRE_NOTHROW( iterator = entity_table->GetIterator( ENTITY_ID_PREFIX "ccc" ) ) )
                  if ( REQUIRE_NOTHROW( element = iterator.Curr() ) )
                  {
                    REQUIRE( element != nullptr );
                    REQUIRE( element->GetId() == ENTITY_ID_PREFIX "ddd" );
                  }
                if ( REQUIRE_NOTHROW( iterator = entity_table->GetIterator( ENTITY_ID_PREFIX "hhh" ) ) )
                  if ( REQUIRE_NOTHROW( element = iterator.Curr() ) )
                  {
                    REQUIRE( element != nullptr );
                    REQUIRE( element->GetId() == ENTITY_ID_PREFIX "hhh" );
                  }
              }
              SECTION( "Next() moves it to the next entity" )
              {
                if ( REQUIRE_NOTHROW( element = iterator.Next() ) )
                  if ( REQUIRE( element != nullptr ) )
                    REQUIRE( element->GetId() == ENTITY_ID_PREFIX "iii" );
              }
              SECTION( "until the end of iterable list." )
              {
                REQUIRE_NOTHROW( element = iterator.Next() );
                REQUIRE( element == nullptr );
              }
            }
          }
        }
        SECTION( "entiry table may be serialized" )
        {
//          entity_table->Serialize( stdout );
        }
      }
      SECTION( "entities table may be created with custom allocator also" )
      {
        auto  entity_arena = mtc::Arena();
        auto  entity_table = entity_arena.Create<dynamic::EntityTable<mtc::Arena::allocator<char>>>( 10000, nullptr );
        auto  entity = mtc::api<IEntity>();

        if ( REQUIRE_NOTHROW( entity = entity_table->SetEntity( "aaa" ) ) )
          if ( REQUIRE( entity != nullptr ) )
            REQUIRE( entity->GetId() != "ccc" );
      }
    }
  } );

