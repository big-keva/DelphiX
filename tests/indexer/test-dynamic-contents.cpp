# include "indexer/dynamic-contents.hpp"
# include "storage/posix-fs.hpp"
# include "src/indexer/dynamic-entities.hpp"
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

TestItEasy::RegisterFunc  dynamic_contents( []()
  {
    TEST_CASE( "index/dynamic-contents" )
    {
      auto  contents = mtc::api<IContentsIndex>();
      auto  entity = mtc::api<const IEntity>();

      SECTION( "dynamic::contents index may be created" )
      {
        REQUIRE_NOTHROW( contents = dynamic::Contents().Create() );

        SECTION( "entities may be inserted to the contents index" )
        {
          contents->SetEntity( "aaa", KeyValues( {
            { "k1", 1161 },
            { "k2", 1262 },
            { "k3", 1263 } } ).ptr() );
          contents->SetEntity( "bbb", KeyValues( {
            { "k2", 1262 },
            { "k3", 1263 },
            { "k4", 1264 } } ).ptr() );
          contents->SetEntity( "ccc", KeyValues( {
            { "k3", 1263 },
            { "k4", 1264 },
            { "k5", 1265 } } ).ptr() );
          contents->DelEntity( "bbb" );
        }
        SECTION( "entities may be get by id" )
        {
          if ( REQUIRE_NOTHROW( entity = contents->GetEntity( "aaa" ) ) )
            if ( REQUIRE( entity != nullptr ) )
              REQUIRE( entity->GetId() == "aaa" );
        }
        SECTION( "entities may be get by index" )
        {
          if ( REQUIRE_NOTHROW( entity = contents->GetEntity( 3 ) ) )
            if ( REQUIRE( entity != nullptr ) )
              REQUIRE( entity->GetId() == "ccc" );
        }
        SECTION( "entities iterators are available" )
        {
          auto  it = mtc::api<IContentsIndex::IEntityIterator>();

          SECTION( "* by index" )
          {
            if ( REQUIRE_NOTHROW( it = contents->GetEntityIterator( 0U ) ) && REQUIRE( it != nullptr ) )
            {
              if ( REQUIRE( it->Curr() != nullptr ) )
                REQUIRE( it->Curr()->GetIndex() == 1U );
              if ( REQUIRE( it->Next() != nullptr ) )
                REQUIRE( it->Curr()->GetIndex() == 3U );
              REQUIRE( it->Next() == nullptr );

              SECTION( "- iterator may start at specified index" )
              {
                if ( REQUIRE_NOTHROW( it = contents->GetEntityIterator( 2U ) ) && REQUIRE( it != nullptr ) )
                {
                  if ( REQUIRE( it->Curr() != nullptr ) )
                    REQUIRE( it->Curr()->GetIndex() == 3U );
                  REQUIRE( it->Next() == nullptr );
                }
              }
            }
          }
          SECTION( "* by id")
          {
            if ( REQUIRE_NOTHROW( it = contents->GetEntityIterator( "" ) ) && REQUIRE( it != nullptr ) )
            {
              if ( REQUIRE( it->Curr() != nullptr ) )
                REQUIRE( it->Curr()->GetId() == "aaa" );
              if ( REQUIRE( it->Next() != nullptr ) )
                REQUIRE( it->Curr()->GetId() == "ccc" );
              REQUIRE( it->Next() == nullptr );

              SECTION( "- iterator may start at specified id" )
              {
                if ( REQUIRE_NOTHROW( it = contents->GetEntityIterator( "bbb" ) ) && REQUIRE( it != nullptr ) )
                {
                  if ( REQUIRE( it->Curr() != nullptr ) )
                    REQUIRE( it->Curr()->GetId() == "ccc" );
                  REQUIRE( it->Next() == nullptr );
                }
              }
            }
          }
        }
        SECTION( "entities are indexed by keys, so keys may be get" )
        {
          mtc::api<IContentsIndex::IEntities>  entities;

          if ( REQUIRE_NOTHROW( contents->GetKeyStats( "k0" ) ) )
            REQUIRE( contents->GetKeyStats( "k0" ).nCount == 0 );
          if ( REQUIRE_NOTHROW( contents->GetKeyStats( "k1" ) ) )
          {
            REQUIRE( contents->GetKeyStats( { "k1", 2 } ).nCount == 1 );
            REQUIRE( contents->GetKeyStats( { "k2", 2 } ).nCount == 2 );
            REQUIRE( contents->GetKeyStats( { "k3", 2 } ).nCount == 3 );
            REQUIRE( contents->GetKeyStats( { "k4", 2 } ).nCount == 2 );
            REQUIRE( contents->GetKeyStats( { "k5", 2 } ).nCount == 1 );
          }

          if ( REQUIRE_NOTHROW( entities = contents->GetKeyBlock( "k3" ) ) )
            if ( REQUIRE( entities != nullptr ) )
            {
              REQUIRE( entities->Find( 0 ).uEntity == 1U );
              REQUIRE( entities->Find( 1 ).uEntity == 1U );
              REQUIRE( entities->Find( 2 ).uEntity == 3U );
              REQUIRE( entities->Find( 3 ).uEntity == 3U );
              REQUIRE( entities->Find( 4 ).uEntity == uint32_t(-1) );
            }
        }
        SECTION( "if dynamic contents index is saved without storage specified, it throws logic_error" )
        {
          REQUIRE_EXCEPTION( contents->Commit(), std::logic_error );
        }
      }
      SECTION( "dynamic::contents may hold extras for entities" )
      {
        REQUIRE_NOTHROW( contents = dynamic::Contents().Create() );

        if ( REQUIRE( contents != nullptr ) )
        {
          SECTION( "extras may be attached do entity on SetEntity()" )
          {
            if ( REQUIRE_NOTHROW( contents->SetEntity( "attached", nullptr, { "aaa", 3 } ) ) )
            {
              if ( REQUIRE_NOTHROW( entity = contents->GetEntity( "attached" ) ) && REQUIRE( entity != nullptr ) )
              {
                if ( REQUIRE( entity->GetExtra() != nullptr ) )
                {
                  REQUIRE( entity->GetExtra()->GetLen() == 3 );
                  REQUIRE( std::string_view( entity->GetExtra()->GetPtr(), 3 ) == "aaa" );
                }
              }
            }
          }
          SECTION( "extras may be changed" )
          {
            if ( REQUIRE_NOTHROW( entity = contents->SetExtras( "attached", { "bbbb", 4 } ) ) )
            {
              if ( REQUIRE( entity != nullptr ) )
              {
                auto  extras = mtc::api<const mtc::IByteBuffer>();

                REQUIRE_NOTHROW( extras = entity->GetExtra() );

                if ( REQUIRE( extras != nullptr ) )
                {
                  REQUIRE( entity->GetExtra()->GetLen() == 4 );
                  REQUIRE( std::string_view( entity->GetExtra()->GetPtr(), 4 ) == "bbbb" );
                }
              }
            }
          }
          SECTION( "setting extras on non-existing documents result nullptr" )
          {
            if ( REQUIRE_NOTHROW( entity = contents->SetExtras( "non-existing", { "bbbb", 4 } ) ) )
              REQUIRE( entity == nullptr );
          }
        }
      }
      SECTION( "dynamic::contents may be created with entity count limitation" )
      {
        REQUIRE_NOTHROW( contents = dynamic::Contents()
          .Set( dynamic::Settings()
            .SetMaxEntities( 3 ) )
          .Create() );

        SECTION( "insertion of more entities than the limit causes count_overflow" )
        {
          REQUIRE_NOTHROW( contents->SetEntity( "aaa", KeyValues( {
            { "aaa", 1161 } } ).ptr() ) );
          REQUIRE_NOTHROW( contents->SetEntity( "bbb", KeyValues( {
            { "bbb", 1161 } } ).ptr() ) );
          REQUIRE_EXCEPTION( contents->SetEntity( "ccc", KeyValues( {
            { "ccc", 1161 } } ).ptr() ), index_overflow );
        }
      }
      SECTION( "dynamic::contents index may be created with size count limitation" )
      {
        REQUIRE_NOTHROW( contents = dynamic::Contents()
          .Set( dynamic::Settings()
            .SetMaxEntities( 3 )
            .SetMaxAllocate( 168880 ) )
          .Create() );

        SECTION( "insertion of more entities than the limit causes count_overflow" )
        {
          REQUIRE_NOTHROW( contents->SetEntity( "aaa", KeyValues( {
            { "aaa", 1161 } } ).ptr() ) );
          REQUIRE_EXCEPTION( contents->SetEntity( "bbb", KeyValues( {
            { "bbb", 1161 } } ).ptr() ), index_overflow );
        }
      }
      SECTION( "created with storage sink, it saves index as static" )
      {
        auto  sink = storage::posixFS::CreateSink( storage::posixFS::StoragePolicies::Open( "/tmp/k2" ) );
        auto  well = mtc::api<IStorage::ISerialized>();

        REQUIRE_NOTHROW( contents = dynamic::Contents()
          .Set( sink )
          .Create() );

        REQUIRE_NOTHROW( contents->SetEntity( "aaa", KeyValues( { { "aaa", 1161 } } ).ptr() ) );
        REQUIRE_NOTHROW( contents->SetEntity( "bbb", KeyValues( { { "bbb", 1162 } } ).ptr() ) );
        REQUIRE_NOTHROW( contents->SetEntity( "ccc", KeyValues( { { "ccc", 1163 } } ).ptr() ) );

        if ( REQUIRE_NOTHROW( well = contents->Commit() ) && REQUIRE( well != nullptr ) )
          REQUIRE_NOTHROW( well->Remove() );
        contents = nullptr;
      }
    }
  } );
