# include "storage/posix-fs.hpp"
# include <mtc/test-it-easy.hpp>
# include <mtc/fileStream.h>
# include <mtc/directory.h>
# include <thread>

using namespace DelphiX;

void  RemoveFiles( const char* path )
{
  auto  dir = mtc::directory::Open( path );

  if ( dir.defined() )
    for ( auto entry = dir.Get(); entry.defined(); entry = dir.Get() )
      remove( mtc::strprintf( "%s%s", entry.folder(), entry.string() ).c_str() );
}

bool  SearchFiles( const char* path )
{
  auto  dir = mtc::directory::Open( path );

  return dir.defined() && dir.Get().defined();
}

TestItEasy::RegisterFunc  storage_fs( []()
  {
    TEST_CASE( "storage/filesystem-based" )
    {
      SECTION( "IStorage::IIndexStore may be created alone" )
      {
        auto  storageSink = mtc::api<IStorage::IIndexStore>();
        auto  outStream = mtc::api<mtc::IByteStream>();

        SECTION( "it may be created by path with default policies" )
        {
          RemoveFiles( "/tmp/k2.*" );

          SECTION( "being called with empty path, id throws std::invalid_argument" )
            {  REQUIRE_EXCEPTION( storage::posixFS::CreateSink( storage::posixFS::StoragePolicies::Open( "" ) ),
              std::invalid_argument );  }
          SECTION( "being called with invalid path, id throws mtc::file_error" )
            {  REQUIRE_EXCEPTION( storage::posixFS::CreateSink( storage::posixFS::StoragePolicies::Open( "/tmp/Palmira/index" ) ),
              mtc::file_error );  }
          SECTION( "being called with correct dir but without generic-name, id throws std::invalid_argument" )
            {  REQUIRE_EXCEPTION( storage::posixFS::CreateSink( storage::posixFS::StoragePolicies::Open( "/tmp/" ) ),
              std::invalid_argument );  }
          SECTION( "being called with correct path, it creates the storage" )
            {  REQUIRE_NOTHROW( storageSink = storage::posixFS::CreateSink( storage::posixFS::StoragePolicies::Open( "/tmp/k2" ) ) );  }
          SECTION( "storage streams   may be accessed..." )
          {
            if ( REQUIRE_NOTHROW( outStream = storageSink->Entities() )
              && REQUIRE( outStream != nullptr ) )
            {
              SECTION( "... and data may be stored" )
                {  REQUIRE( outStream->Put( "some sample data", 16 ) == 16 );  }
            }
          }
          SECTION( "being closed without Commit() call, it removes all the created files" )
          {
            if ( REQUIRE_NOTHROW( storageSink = nullptr ) )
              REQUIRE( SearchFiles( "/tmp/k2.*" ) == false );
          }
          SECTION( "storage may be commited, status file is created" )
          {
            RemoveFiles( "/tmp/k2.*" );

            REQUIRE_NOTHROW( storageSink = storage::posixFS::CreateSink( storage::posixFS::StoragePolicies::Open( "/tmp/k2" ) ) );
            REQUIRE_NOTHROW( outStream = storageSink->Entities() );
            REQUIRE_NOTHROW( storageSink->Commit() );
            REQUIRE_NOTHROW( storageSink = nullptr );

            REQUIRE( SearchFiles( "/tmp/k2.*" ) );
            REQUIRE( SearchFiles( "/tmp/k2.*.status" ) );

            RemoveFiles( "/tmp/k2.*" );
}
        }
      }
    }
  } );
