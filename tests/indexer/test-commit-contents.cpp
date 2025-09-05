# include "../../indexer/dynamic-contents.hpp"
# include "../../storage/posix-fs.hpp"
# include "../../src/indexer/commit-contents.hpp"
# include <mtc/recursive_shared_mutex.hpp>
# include <mtc/test-it-easy.hpp>
# include <mtc/zmap.h>
# include <condition_variable>
# include <thread>
# include <chrono>

using namespace DelphiX;

class MockBuffer: public mtc::IByteBuffer
{
  implement_lifetime_stub

  const char* GetPtr() const override {  return "";  }
  size_t      GetLen() const override {  return 0;  }
  int         SetBuf( const void*, size_t ) override {  throw std::logic_error( "not implemented @" __FILE__ ":" LINE_STRING );  }
  int         SetLen( size_t ) override {  throw std::logic_error( "not implemented @" __FILE__ ":" LINE_STRING );  }
};

class MockPatch: public IStorage::ISerialized::IPatch
{
  implement_lifetime_stub

public:
  void  Delete( EntityId ) override {}
  void  Update( EntityId, const void*, size_t ) override {}
  void  Commit() override {}

};

class MockSeralized: public IStorage::ISerialized
{
  MockBuffer  buffer;
  MockPatch   patch;

  implement_lifetime_stub

  auto  Entities() -> mtc::api<const mtc::IByteBuffer> override {  return &buffer;  }
  auto  Contents() -> mtc::api<const mtc::IByteBuffer> override {  return &buffer;  }
  auto  Linkages() -> mtc::api<mtc::IFlatStream> override {  return nullptr;  }

  auto  Commit() -> mtc::api<ISerialized> override  {  return this;  }
  void  Remove() override {}

  auto  NewPatch() -> mtc::api<IPatch> override {  return &patch;  }

};

class MockEntity: public IEntity
{
  implement_lifetime_stub

  auto  GetId() const -> EntityId override {  return { "dynamicId" };  }
  auto  GetIndex() const -> uint32_t override {  return 3;  }
  auto  GetExtra() const -> mtc::api<const mtc::IByteBuffer> override  {  return {};  }
  auto  GetVersion() const -> uint64_t override {  return 0; }
};

class MockCommitable: public IContentsIndex
{
  std::chrono::milliseconds millis;
  std::function<void()>     signal;

  MockSeralized seralized;
  MockEntity entity;

  implement_lifetime_stub

  MockCommitable( std::chrono::milliseconds wait, std::function<void()> notf ):
    millis( wait ), signal( notf )  {}

public:
  auto  GetEntity( EntityId id ) const -> mtc::api<const IEntity>
    {  return id == "dynamicId" ? &entity : nullptr;  }
  auto  GetEntity( uint32_t ix ) const -> mtc::api<const IEntity>
    {  return ix == 3 ? &entity : nullptr;  }
  bool  DelEntity( EntityId )
    {  throw std::logic_error( "invalid call" );  }
  auto  SetEntity( EntityId, mtc::api<const IContents>, const StrView& ) -> mtc::api<const IEntity> override
    {  throw std::logic_error( "invalid call" );  }
  auto  SetExtras( EntityId, const StrView& ) -> mtc::api<const IEntity> override
    {  throw std::logic_error( "invalid call" );  }
  auto  GetMaxIndex() const -> uint32_t   {  return 1000;  }
  auto  GetKeyBlock( const StrView& ) const -> mtc::api<IEntities>
    {  throw std::logic_error( "invalid call" );  }
  auto  GetKeyStats( const StrView& ) const -> BlockInfo
    {  return { 0, 5 };  }
  auto  ListEntities( EntityId ) -> mtc::api<IEntitiesList> override
    {  throw std::runtime_error( "not implemented @" __FILE__ ":" LINE_STRING );  }
  auto  ListEntities( uint32_t ) -> mtc::api<IEntitiesList> override
    {  throw std::runtime_error( "not implemented @" __FILE__ ":" LINE_STRING );  }
  auto  ListContents( const StrView& ) -> mtc::api<IContentsList> override
    {  throw std::runtime_error( "not implemented @" __FILE__ ":" LINE_STRING );  }
  auto  Commit() -> mtc::api<IStorage::ISerialized>
    {
      std::this_thread::sleep_for( millis );
      signal();
      return &seralized;
    }
  auto  Reduce() -> mtc::api<IContentsIndex>
    {  throw std::logic_error( "invalid call" );  }
  void  Remove() override
    {  throw std::logic_error( "invalid call" );  }
  void  Stash( EntityId ) override
    {}

};

TestItEasy::RegisterFunc  commit_contents( []()
  {
    TEST_CASE( "index/commit-contents" )
    {
      SECTION( "commit::contents index is created above dynamic contents to serialize it" )
      {
        SECTION( "it calls passed index::Commit() asynchronously" )
        {
          std::mutex              mutex;
          auto                    xlock = mtc::make_unique_lock( mutex );
          std::condition_variable cwait;
          bool                    fdone = false;
          MockCommitable          amock( std::chrono::milliseconds( 100 ), [&]()
            {  fdone = true;  cwait.notify_one();  } );
          auto                    index = indexer::commit::Contents().Create( &amock, {} );

          SECTION( "SetEntity(...) must not be called causes std::logic_error" )
            {  REQUIRE_EXCEPTION( index->SetEntity( "aaa" ), std::logic_error );  }
          SECTION( "GetMaxIndex() is forwarded to index being commited" )
            {  REQUIRE( index->GetMaxIndex() == 1000 );  }
          SECTION( "GetKeyStats() is forwarded to index being commited" )
            {  REQUIRE( index->GetKeyStats( "aaa" ).nCount == 5 );  }
          SECTION( "GetEntity(...) forwards call to commited index before commit is finished" )
          {
            if ( REQUIRE_NOTHROW( index->GetEntity( "aaa" ) ) )
              REQUIRE( index->GetEntity( "aaa" ) == nullptr );
            if ( REQUIRE_NOTHROW( index->GetEntity( 1 ) ) )
              REQUIRE( index->GetEntity( 1 ) == nullptr );

            if ( REQUIRE_NOTHROW( index->GetEntity( "dynamicId" ) ) )
              if ( REQUIRE( index->GetEntity( "dynamicId" ) != nullptr ) )
                REQUIRE( index->GetEntity( "dynamicId" )->GetId() == "dynamicId" );
            if ( REQUIRE_NOTHROW( index->GetEntity( 3 ) ) )
              if ( REQUIRE( index->GetEntity( 3 ) != nullptr ) )
                REQUIRE( index->GetEntity( 3 )->GetIndex() == 3 );
          }
          SECTION( "entities may be deleted from the commited index" )
          {
            bool  deleted;

            SECTION( "deleting non-existing documents return false" )
            {
              if ( REQUIRE_NOTHROW( deleted = index->DelEntity( "aaa" ) ) )
                REQUIRE( !deleted );
            }
            SECTION( "deleting existing documents return true" )
            {
              if ( REQUIRE_NOTHROW( deleted = index->DelEntity( "dynamicId" ) ) )
              {
                REQUIRE( deleted );

                SECTION( "after deleting entities may not be found" )
                {
                  if ( REQUIRE_NOTHROW( index->GetEntity( "dynamicId" ) ) )
                  {
                    REQUIRE( index->GetEntity( "dynamicId" ) == nullptr );
                    REQUIRE( index->GetEntity( 3 ) == nullptr );
                  }
                }
              }
            }
          }
          REQUIRE( !fdone );
          REQUIRE( cwait.wait_for( xlock, std::chrono::milliseconds( 200 ) ) == std::cv_status::no_timeout );
          REQUIRE( fdone );
        }
      }

    }
  } );
