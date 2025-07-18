# include "../../storage/posix-fs.hpp"
# include <mtc/fileStream.h>
# include <mtc/wcsstr.h>
# include <stdexcept>
# include <thread>
# include <unistd.h>

namespace DelphiX {
namespace storage {
namespace posixFS {

  class Serialized final: public IStorage::ISerialized
  {
    implement_lifetime_control

  public:
    Serialized( const StoragePolicies& pol ):
      policies( pol ) {}

  public:
    auto  Entities() -> mtc::api<const mtc::IByteBuffer> override;
    auto  Contents() -> mtc::api<const mtc::IByteBuffer> override;
    auto  Blocks() -> mtc::api<mtc::IFlatStream> override;

    auto  Commit() -> mtc::api<ISerialized> override;
    void  Remove() override;

    auto  NewPatch() -> mtc::api<IPatch> override;

  protected:
    const StoragePolicies                 policies;

    mtc::api<const mtc::IByteBuffer>      entities;
    mtc::api<const mtc::IByteBuffer>      contents;
    mtc::api<      mtc::IFlatStream>      blocks;

  };

  auto  LoadByteBuffer( const StoragePolicies& policies, Unit unit ) -> mtc::api<const mtc::IByteBuffer>
  {
    auto  policy = policies.GetPolicy( unit );

    if ( policy != nullptr )
    {
      auto  infile = mtc::OpenFileStream( policy->GetFilePath( unit ).c_str(), O_RDONLY,
        mtc::enable_exceptions );

    // check the signature

    // if preloaded, return preloaded buffer, else memory-mapped
      if ( policy->mode == preloaded )
        return infile->PGet( 0, infile->Size() - 0 ).ptr();
      if ( policy->mode == memory_mapped )
        return infile->MemMap( 0, infile->Size() - 0 ).ptr();
      throw std::invalid_argument( "invalid open mode" );
    }
    return nullptr;
  }

  // Serialized implementation

 /*
  * Serialized::Entities()
  *
  * Loads and returns the byte buffer for entities table access.
  */
  auto  Serialized::Entities() -> mtc::api<const mtc::IByteBuffer>
  {
    return LoadByteBuffer( policies, Unit::entities );
  }

  auto  Serialized::Contents() -> mtc::api<const mtc::IByteBuffer>
  {
    return LoadByteBuffer( policies, Unit::contents );
  }

  auto  Serialized::Blocks() -> mtc::api<mtc::IFlatStream>
  {
    return blocks = mtc::OpenFileStream( policies.GetPolicy( Unit::blocks )->GetFilePath( Unit::blocks ).c_str(),
      O_RDONLY, mtc::enable_exceptions ).ptr();
  }

  auto  Serialized::Commit() -> mtc::api<ISerialized>
  {
    return this;
  }

  void  Serialized::Remove()
  {
    entities = nullptr;  blocks = nullptr;
    contents = nullptr;

    for ( auto unit: { Unit::entities, Unit::blocks, Unit::contents, Unit::images, Unit::status } )
    {
      auto  policy = policies.GetPolicy( unit );

      if ( policy != nullptr )
        remove( policy->GetFilePath( unit ).c_str() );
    }
  }

  auto  Serialized::NewPatch() -> mtc::api<IPatch>
  {
    return nullptr;
  }

  auto  OpenSerial( const StoragePolicies& policies ) -> mtc::api<IStorage::ISerialized>
  {
    return new Serialized( policies );
  }

}}}
