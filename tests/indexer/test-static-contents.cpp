# include "indexer/dynamic-contents.hpp"
# include "indexer/static-contents.hpp"
# include "src/indexer/dynamic-entities.hpp"
# include "storage/posix-fs.hpp"
# include <mtc/test-it-easy.hpp>
# include <mtc/zmap.h>

using namespace DelphiX;
using namespace DelphiX::indexer;

class KeyValues: public IContents, protected mtc::zmap
{
  implement_lifetime_stub

public:
  KeyValues( const mtc::zmap& keyval ):
    zmap( keyval )  {}

  auto  ptr() const -> const IContents*
    {  return this;  }

  void  Enum( IContentsIndex::IIndexAPI* to ) const override
    {
      for ( auto keyvalue: *this )
      {
        auto  val = keyvalue.second.to_string();

        to->Insert( { (const char*)keyvalue.first.data(), keyvalue.first.size() },
          { val.data(), val.size() }, unsigned(-1) );
      }
    }
};

TestItEasy::RegisterFunc  static_contents( []()
  {
    TEST_CASE( "index/static-contents" )
    {
      SECTION( "static::contents index is created from serialized dynamic contents" )
      {
        auto  contents = mtc::api<IContentsIndex>();
        auto  serialized = mtc::api<IStorage::ISerialized>();

        REQUIRE_NOTHROW( contents = dynamic::Contents()
          .Set( storage::posixFS::CreateSink( storage::posixFS::StoragePolicies::Open( "/tmp/k2" ) ) ).Create() );
        REQUIRE_NOTHROW( contents->SetEntity( "aaa", KeyValues( {
            { "aaa", 1161 },
            { "bbb", 1262 },
            { "ccc", 1263 } } ).ptr() ) );
        REQUIRE_NOTHROW( contents->SetEntity( "bbb", KeyValues( {
            { "bbb", 1262 },
            { "ccc", 1263 },
            { "ddd", 1264 } } ).ptr() ) );
        REQUIRE_NOTHROW( contents->SetEntity( "ccc", KeyValues( {
            { "ccc", 1263 },
            { "ddd", 1264 },
            { "eee", 1265 } } ).ptr() ) );
        REQUIRE_NOTHROW( contents->DelEntity( "bbb" ) );
        REQUIRE_NOTHROW( serialized = contents->Commit() );

        if ( REQUIRE_NOTHROW( contents = static_::Contents().Create( serialized ) )
          && REQUIRE( contents != nullptr ) )
        {
          mtc::api<IContentsIndex::IEntities>  entities;
          mtc::api<const IEntity>  entity;

          SECTION( "entities may be get" )
          {
            SECTION( "* by id" )
            {
              if ( REQUIRE_NOTHROW( entity = contents->GetEntity( "aaa" ) ) && REQUIRE( entity != nullptr ) )
                REQUIRE( entity->GetId() == "aaa" );
              if ( REQUIRE_NOTHROW( entity = contents->GetEntity( "bbb" ) ) )
                REQUIRE( entity == nullptr );
              if ( REQUIRE_NOTHROW( entity = contents->GetEntity( "ccc" ) ) && REQUIRE( entity != nullptr ) )
                REQUIRE( entity->GetId() == "ccc" );
            }
            SECTION( "* by index" )
            {
              if ( REQUIRE_NOTHROW( entity = contents->GetEntity( 1U ) ) && REQUIRE( entity != nullptr ) )
              {
                REQUIRE( entity->GetIndex() == 1 );
                REQUIRE( entity->GetId() == "aaa" );
              }
              if ( REQUIRE_NOTHROW( entity = contents->GetEntity( 2U ) ) )
                REQUIRE( entity != nullptr );
              if ( REQUIRE_NOTHROW( entity = contents->GetEntity( 3U ) ) && REQUIRE( entity != nullptr ) )
              {
                REQUIRE( entity->GetIndex() == 3 );
                REQUIRE( entity->GetId() == "ccc" );
              }
            }
          }
          SECTION( "entities are iterable" )
          {
            auto  it = mtc::api<IContentsIndex::IEntityIterator>();

            SECTION( "* by id" )
            {
              if ( REQUIRE_NOTHROW( it = contents->GetEntityIterator( "" ) ) && REQUIRE( it != nullptr ) )
              {
                if ( REQUIRE_NOTHROW( entity = it->Curr() ) && REQUIRE( entity != nullptr ) )
                  REQUIRE( entity->GetId() == "aaa" );
                if ( REQUIRE_NOTHROW( entity = it->Next() ) && REQUIRE( entity != nullptr ) )
                  REQUIRE( entity->GetId() == "ccc" );
                if ( REQUIRE_NOTHROW( entity = it->Next() ) )
                  REQUIRE( entity == nullptr );

                SECTION( "- it may be positioned to some entity on create" )
                {
                  if ( REQUIRE_NOTHROW( it = contents->GetEntityIterator( "b" ) ) && REQUIRE( it != nullptr ) )
                    if ( REQUIRE_NOTHROW( entity = it->Curr() ) && REQUIRE( entity != nullptr ) )
                      REQUIRE( entity->GetId() == "ccc" );
                }
              }
            }
            SECTION( "* by index" )
            {
              if ( REQUIRE_NOTHROW( it = contents->GetEntityIterator( 0U ) ) && REQUIRE( it != nullptr ) )
              {
                if ( REQUIRE_NOTHROW( entity = it->Curr() ) && REQUIRE( entity != nullptr ) )
                  REQUIRE( entity->GetIndex() == 1 );
                if ( REQUIRE_NOTHROW( entity = it->Next() ) && REQUIRE( entity != nullptr ) )
                  REQUIRE( entity->GetIndex() == 3 );
                if ( REQUIRE_NOTHROW( entity = it->Next() ) )
                  REQUIRE( entity == nullptr );
              }
            }
          }
          SECTION( "key statistics is available" )
          {
            if ( REQUIRE_NOTHROW( contents->GetKeyStats( "aaa" ) ) )
              REQUIRE( contents->GetKeyStats( "aaa" ).nCount == 1 );
            if ( REQUIRE_NOTHROW( contents->GetKeyStats( "bbb" ) ) )
              REQUIRE( contents->GetKeyStats( "bbb" ).nCount == 1 );
            if ( REQUIRE_NOTHROW( contents->GetKeyStats( "ccc" ) ) )
              REQUIRE( contents->GetKeyStats( "ccc" ).nCount == 2 );
            if ( REQUIRE_NOTHROW( contents->GetKeyStats( "ddd" ) ) )
              REQUIRE( contents->GetKeyStats( "ddd" ).nCount == 1 );
            if ( REQUIRE_NOTHROW( contents->GetKeyStats( "eee" ) ) )
              REQUIRE( contents->GetKeyStats( "eee" ).nCount == 1 );
            if ( REQUIRE_NOTHROW( contents->GetKeyStats( "fff" ) ) )
              REQUIRE( contents->GetKeyStats( "fff" ).nCount == 0 );
          }
          SECTION( "key blocks are available" )
          {
            IContentsIndex::IEntities::Reference entRef;

            if ( REQUIRE_NOTHROW( entities = contents->GetKeyBlock( "aaa" ) ) && REQUIRE( entities != nullptr ) )
            {
              if ( REQUIRE_NOTHROW( entRef = entities->Find( 1 ) ) )
              {
                REQUIRE( entRef.uEntity == 1 );
                if ( REQUIRE_NOTHROW( entRef = entities->Find( entRef.uEntity + 1 ) ) )
                  REQUIRE( entRef.uEntity == uint32_t(-1) );
              }
            }

            if ( REQUIRE_NOTHROW( contents->GetKeyStats( "bbb" ) ) )
              REQUIRE( contents->GetKeyStats( "bbb" ).nCount == 1 );
            if ( REQUIRE_NOTHROW( contents->GetKeyStats( "ccc" ) ) )
              REQUIRE( contents->GetKeyStats( "ccc" ).nCount == 2 );
            if ( REQUIRE_NOTHROW( contents->GetKeyStats( "ddd" ) ) )
              REQUIRE( contents->GetKeyStats( "ddd" ).nCount == 1 );
            if ( REQUIRE_NOTHROW( contents->GetKeyStats( "eee" ) ) )
              REQUIRE( contents->GetKeyStats( "eee" ).nCount == 1 );
            if ( REQUIRE_NOTHROW( contents->GetKeyStats( "fff" ) ) )
              REQUIRE( contents->GetKeyStats( "fff" ).nCount == 0 );
          }
          SECTION( "entity may be stashed" )
          {
            REQUIRE_NOTHROW( contents->Stash( "aaa" ) );

            SECTION( "stashed entities are invisible" )
            {

              SECTION( "for GetEntity()" )
              {
                if ( REQUIRE_NOTHROW( entity = contents->GetEntity( "aaa" ) ) )
                  REQUIRE( entity == nullptr );
                if ( REQUIRE_NOTHROW( entity = contents->GetEntity( 1U ) ) )
                  REQUIRE( entity == nullptr );
              }
              SECTION( "for iterators" )
              {
                auto  it = mtc::api<IContentsIndex::IEntityIterator>();

                SECTION( "* by id" )
                {
                  if ( REQUIRE_NOTHROW( it = contents->GetEntityIterator( "aaa" ) ) && REQUIRE( it != nullptr ) )
                    if ( REQUIRE_NOTHROW( entity = it->Curr() ) && REQUIRE( entity != nullptr ) )
                      REQUIRE( entity->GetId() == "ccc" );
                }
                SECTION( "* by index" )
                {
                  auto  it = mtc::api<IContentsIndex::IEntityIterator>();

                  if ( REQUIRE_NOTHROW( it = contents->GetEntityIterator( 0U ) ) && REQUIRE( it != nullptr ) )
                    if ( REQUIRE_NOTHROW( entity = it->Curr() ) && REQUIRE( entity != nullptr ) )
                      REQUIRE( entity->GetIndex() == 3 );
                }
              }
              SECTION( "for key blocks" )
              {
                IContentsIndex::IEntities::Reference entRef;

                REQUIRE_NOTHROW( entities = contents->GetKeyBlock( "ccc" ) );
                REQUIRE( entities != nullptr );

                if ( REQUIRE_NOTHROW( entRef = entities->Find( 1 ) ) )
                  REQUIRE( entRef.uEntity == 3 );
              }
            }
          }
          SECTION( "entities extras may be changed" )
          {
            auto  extras = mtc::api<const mtc::IByteBuffer>();

            REQUIRE_NOTHROW( entity = contents->SetExtras( "ccc", { "cccc", 4 } ) );

            if ( REQUIRE( entity != nullptr ) )
            {
              REQUIRE_NOTHROW( extras = entity->GetExtra() );

              if ( REQUIRE( extras != nullptr ) )
              {
                REQUIRE( extras->GetLen() == 4 );
                REQUIRE( std::string_view( extras->GetPtr(), 4 ) == "cccc" );
              }
            }

            SECTION( "after setting, extras are changed permanent" )
            {
              entity = contents->GetEntity( "ccc" );

              if ( REQUIRE( entity != nullptr ) )
              {
                REQUIRE_NOTHROW( extras = entity->GetExtra() );

                if ( REQUIRE( extras != nullptr ) )
                {
                  REQUIRE( extras->GetLen() == 4 );
                  REQUIRE( std::string_view( extras->GetPtr(), 4 ) == "cccc" );
                }
              }
            }
            SECTION( "access by iterator also is affected by changing extras" )
            {
              auto  it = mtc::api<IContentsIndex::IEntityIterator>();

              REQUIRE_NOTHROW( it = contents->GetEntityIterator( "ccc" ) );
              if ( REQUIRE( it != nullptr ) )
              {
                REQUIRE_NOTHROW( entity = it->Curr() );
                if ( REQUIRE( entity != nullptr ) )
                {
                  REQUIRE_NOTHROW( extras = entity->GetExtra() );
                  if ( REQUIRE( extras != nullptr ) )
                  {
                    REQUIRE( extras->GetLen() == 4 );
                    REQUIRE( std::string_view( extras->GetPtr(), 4 ) == "cccc" );
                  }
                }
              }
            }
          }
          SECTION( "entites may be deleted" )
          {
            bool  deleted;

            if ( REQUIRE_NOTHROW( deleted = contents->DelEntity( "ccc" ) ) )
              REQUIRE( deleted == true );

            SECTION( "after deletion entity may not be found" )
            {
              SECTION( "- by id" )
              {
                if ( REQUIRE_NOTHROW( contents->GetEntity( "ccc" ) ) )
                  REQUIRE( contents->GetEntity( "ccc" ) == nullptr );
              }
              SECTION( "- by index" )
              {
                if ( REQUIRE_NOTHROW( contents->GetEntity( 3 ) ) )
                  REQUIRE( contents->GetEntity( 3 ) == nullptr );
              }
              SECTION( "- by contents" )
              {
                IContentsIndex::IEntities::Reference entRef;

                REQUIRE_NOTHROW( entities = contents->GetKeyBlock( "eee" ) );
                REQUIRE( entities != nullptr );

                if ( REQUIRE_NOTHROW( entRef = entities->Find( 3 ) ) )
                  REQUIRE( entRef.uEntity == uint32_t(-1) );
              }
            }
            SECTION( "second deletion of deleted object returns false" )
            {
              REQUIRE( contents->DelEntity( "ccc" ) == false );
            }
          }
        }
      }
    }
  } );
