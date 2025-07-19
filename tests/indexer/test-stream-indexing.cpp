# include "../../indexer/layered-contents.hpp"
# include "../../storage/posix-fs.hpp"
# include <mtc/test-it-easy.hpp>
# include <mtc/zmap.h>
# include <thread>

using namespace DelphiX;
using namespace DelphiX::indexer;

class Contents: public IContents, protected std::vector<std::pair<std::string, std::string>>
{
  implement_lifetime_stub

  friend Contents CreateContents();

  void  Enum( IContentsIndex::IIndexAPI* index ) const override
  {
    for ( auto& next: *this )
      index->Insert( next.first, next.second, 1 );
  }
};

auto  CreateContents() -> Contents
{
  Contents  out;

  auto  nKeys = size_t(100 + rand() * 300. / RAND_MAX);
  auto  nStep = size_t(1 + rand() * 2. / RAND_MAX);

  for ( size_t i = 0; i < nKeys; i += nStep )
    out.emplace_back( mtc::strprintf( "key%u", i ), mtc::strprintf( "val%u", i ) );

  return out;
}

TestItEasy::RegisterFunc  stream_indexing( []()
  {
    TEST_CASE( "index/stream-indexing" )
    {
      auto  storage = storage::posixFS::Open( storage::posixFS::StoragePolicies::Open( "/tmp/k2" ) );
      auto  layered = layered::Index()
        .Set( storage )
        .Set( dynamic::Settings()
          .SetMaxEntities( 4096 )
          .SetMaxAllocate( 256 * 1024 * 1024 ) )
        .Create();

      SECTION( "indexing a set of entities generates a set of indices" )
      {
        std::vector<std::thread>  threads;

        for ( int i = 0; i < 10; ++i )
          threads.emplace_back( std::thread( [&]( int base )
          {
            for ( auto entId = base * 1000; entId < (base + 1) * 1000; ++entId )
            {
              auto  contents = CreateContents();

              layered->SetEntity( std::string_view( mtc::strprintf( "ent%u", entId ) ), &contents );
            }
          }, i ) );
        for ( auto& th: threads )
          th.join();
      }
    }
  } );
