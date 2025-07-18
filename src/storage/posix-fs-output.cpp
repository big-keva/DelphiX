# include "storage/posix-fs.hpp"
# include <mtc/fileStream.h>
# include <mtc/wcsstr.h>
# include <functional>
# include <stdexcept>
# include <thread>
# include <unistd.h>

namespace DelphiX {
namespace storage {
namespace posixFS {

  class Sink final: public IStorage::IIndexStore
  {
    std::atomic_long  referenceCounter = 0;

    friend auto CreateSink( const StoragePolicies& ) -> mtc::api<IIndexStore>;

  public:
    Sink( const StoragePolicies& s ):
      policies( s )  {}
    Sink( Sink&& sink ):
      policies( std::move( sink.policies ) ),
      autoRemove( std::move( sink.autoRemove ) ),
      entities( std::move( sink.entities ) ),
      contents( std::move( sink.contents ) ),
      blocks( std::move( sink.blocks ) )  {}
    Sink( const Sink& ) = delete;

  protected:  // lifetime control
    long  Attach() override {  return ++referenceCounter;  }
    long  Detach() override;

  public:
    auto  Entities() -> mtc::api<mtc::IByteStream> override {  return entities;  }
    auto  Contents() -> mtc::api<mtc::IByteStream> override  {  return contents;  }
    auto  Chains() -> mtc::api<mtc::IByteStream> override {  return blocks;  }

    auto  Commit() -> mtc::api<IStorage::ISerialized> override;
    void  Remove() override;

  protected:
    StoragePolicies                 policies;
    bool                            autoRemove = true;

    mtc::api<mtc::IByteStream>      entities;
    mtc::api<mtc::IByteStream>      contents;
    mtc::api<mtc::IByteStream>      blocks;

  };

  // Sink implementation

  long  Sink::Detach()
  {
    auto  rcount = --referenceCounter;

    if ( rcount == 0 )
    {
      entities = nullptr;  blocks = nullptr;
      contents = nullptr;

      if ( autoRemove )
        Sink::Remove();

      delete this;
    }
    return rcount;
  }

  auto  Sink::Commit() -> mtc::api<IStorage::ISerialized>
  {
    auto  policy = policies.GetPolicy( status );

    if ( policy == nullptr )
      throw std::invalid_argument( "policy does not contain record for '.stats' file" );

    close(
      open( policy->GetFilePath( status ).c_str(), O_CREAT + O_RDWR, 0644 ) );

    autoRemove = false;

    return OpenSerial( policies );
  }

  void  Sink::Remove()
  {
    entities = nullptr;  blocks = nullptr;
    contents = nullptr;

    for ( auto unit: { Unit::entities, Unit::blocks, Unit::contents, Unit::status } )
    {
      auto  policy = policies.GetPolicy( unit );

      if ( policy != nullptr )
        remove( policy->GetFilePath( unit ).c_str() );
    }
  }

  template <class It> static
  bool  CaptureFiles( It unitBeg, It unitEnd, const char* stump, const StoragePolicies& policies, bool forced )
  {
    auto  policy = policies.GetPolicy( *unitBeg );
    int   nerror;

    if ( policy != nullptr )
    {
      auto  unitPath = policy->GetFilePath( *unitBeg, stump );
      auto  f_handle = open( unitPath.c_str(), O_CREAT + O_RDWR + (forced ? 0 : O_EXCL), 0644 );

    // check if open error or file already exists
      if ( f_handle < 0 )
      {
        if ( (nerror = errno) != EEXIST )
        {
          throw mtc::file_error( mtc::strprintf( "could not create file '%s', error %d (%s)",
            unitPath.c_str(), nerror, strerror( nerror ) ) );
        }
        return false;
      } else close( f_handle );

    // file is opened and closed; continue with files
      if ( ++unitBeg >= unitEnd )
        return true;

    // try create the other files
      if ( CaptureFiles( unitBeg, unitEnd, stump, policies, forced ) )
        return true;

      return remove( unitPath.c_str() ), false;
    }
    throw std::invalid_argument( mtc::strprintf( "undefined open policy for '%s'",
      StoragePolicies::GetSuffix( *unitBeg ) ) );
  }

  static
  auto  CaptureIndex( const std::initializer_list<Unit> units, const StoragePolicies& policies, bool forced ) -> std::string
  {
    for ( ; ; std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) ) )
    {
      auto  tm = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch() ).count();
      char  st[64];

      sprintf( st, "%lu", tm );

      if ( !CaptureFiles( units.begin(), units.end(), st, policies, forced ) )
        continue;

      return st;
    }
  }

  auto  CreateSink( const StoragePolicies& policies ) -> mtc::api<IStorage::IIndexStore>
  {
    auto  units = std::initializer_list<Unit>{ entities, contents, blocks };
    auto  stamp = CaptureIndex( units, policies, policies.IsInstance() );
    Sink  aSink( policies.GetInstance( stamp ) );

  // OK, the list of files is captured; create the sink
    aSink.entities = mtc::OpenFileStream( aSink.policies.GetPolicy( entities )->GetFilePath( entities ).c_str(), O_RDWR );
    aSink.contents = mtc::OpenFileStream( aSink.policies.GetPolicy( contents )->GetFilePath( contents ).c_str(), O_RDWR );
    aSink.blocks = mtc::OpenFileStream( aSink.policies.GetPolicy( blocks )->GetFilePath( blocks ).c_str(), O_RDWR );
//    aSink.images   = mtc::OpenFileStream( policies.GetPolicy( images )->GetFilePath( images, stamp ).c_str(), O_RDWR );

    return new Sink( std::move( aSink ) );
  }

}}}
