# include "../../storage/posix-fs.hpp"
# include "../../compat.hpp"
# include "posix-fs-dump-store.hpp"
# include <mtc/fileStream.h>
# include <mtc/wcsstr.h>
# include <stdexcept>

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
    auto  Linkages() -> mtc::api<mtc::IFlatStream> override;
    auto  Packages() -> mtc::api<IStorage::IDumpStore> override;
    auto  Commit() -> mtc::api<ISerialized> override;
    void  Remove() override;

    auto  NewPatch() -> mtc::api<IPatch> override;

  protected:
    const StoragePolicies                 policies;

    mtc::api<const mtc::IByteBuffer>      entities;
    mtc::api<const mtc::IByteBuffer>      contents;
    mtc::api<      mtc::IFlatStream>      linkages;
    mtc::api<IStorage::IDumpStore>        packages;

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
      {
        if ( infile->Size() > (std::numeric_limits<uint32_t>::max)() )
          throw std::invalid_argument( "file too large to be preloaded @" __FILE__ ":" LINE_STRING );
        return infile->PGet( 0, uint32_t(infile->Size() - 0) ).ptr();
      }
      if ( policy->mode == memory_mapped )
        return infile->MemMap( 0, infile->Size() - 0 ).ptr();
      throw std::invalid_argument( "invalid open mode @" __FILE__ ":" LINE_STRING );
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
    if ( entities == nullptr )
      entities = LoadByteBuffer( policies, Unit::entities );
    return entities;
  }

  auto  Serialized::Contents() -> mtc::api<const mtc::IByteBuffer>
  {
    if ( contents == nullptr )
      contents = LoadByteBuffer( policies, Unit::contents );
    return contents;
  }

  auto  Serialized::Linkages() -> mtc::api<mtc::IFlatStream>
  {
    if ( linkages == nullptr )
    {
      linkages = mtc::OpenFileStream( policies.GetPolicy( Unit::linkages )->GetFilePath( Unit::linkages ).c_str(),
        O_RDONLY, mtc::enable_exceptions ).ptr();
    }
    return linkages;
  }

  auto  Serialized::Packages() -> mtc::api<IStorage::IDumpStore>
  {
    if ( packages == nullptr )
    {
      packages = CreateDumpStore( mtc::OpenFileStream( policies.GetPolicy( Unit::packages )->GetFilePath( Unit::packages ).c_str(),
        O_RDONLY, mtc::disable_exceptions ).ptr() );
    }
    return packages;
  }

  auto  Serialized::Commit() -> mtc::api<ISerialized>
  {
    return this;
  }

  void  Serialized::Remove()
  {
    entities = nullptr;
    linkages = nullptr;
    contents = nullptr;
    packages = nullptr;

    for ( auto unit: { Unit::entities, Unit::linkages, Unit::contents, Unit::packages, Unit::bulletin } )
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
