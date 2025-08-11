# include "../../storage/posix-fs.hpp"
# include <mtc/directory.h>
# include <algorithm>

namespace DelphiX {
namespace storage {
namespace posixFS {

  class Storage final: public IStorage
  {
    implement_lifetime_control

    class SourceList;

  public:
    Storage( const StoragePolicies& pols ):
      policies( pols )  {}
  public:
    auto  ListIndices() -> mtc::api<ISourceList> override;
    auto  CreateStore() -> mtc::api<IIndexStore> override;

  protected:
    StoragePolicies policies;

  };

  class Storage::SourceList final: public ISourceList
  {
    using PolicyElements = std::vector<StoragePolicies>;
    using PolicyIterator = PolicyElements::const_iterator;

    implement_lifetime_control

  protected:
    auto  Get() -> mtc::api<ISerialized> override;

  public:
    SourceList( std::vector<StoragePolicies>&& instances ):
      policies( std::move( instances ) ),
      iterator( policies.begin() )  {}

  protected:
    PolicyElements  policies;
    PolicyIterator  iterator;

  };

  // Storage::SourceList implementation

  auto Storage::SourceList::Get() -> mtc::api<ISerialized>
  {
    if ( iterator != policies.end() )
      return OpenSerial( *iterator++ );
    return nullptr;
  }

  // Storage implementation

  auto  Storage::ListIndices() -> mtc::api<ISourceList>
  {
    auto  theInstances = std::vector<StoragePolicies>();
    auto  statusPolicy = policies.GetPolicy( Unit::status );
    auto  pathTemplate = std::string();
    auto  theDirectory = mtc::directory();

    if ( statusPolicy == nullptr )
      return nullptr;

    pathTemplate = statusPolicy->GetFilePath( Unit::status, "*" );
    theDirectory = mtc::directory::Open( pathTemplate.c_str(), mtc::directory::attr_file );

    if ( policies.IsInstance() )
    {
      theInstances.emplace_back( policies );
    }
      else
    {
      auto  asteriskOffs = pathTemplate.find_last_of( '*' );
      auto  sortedSuffix = std::vector<std::string>();

      if ( theDirectory.defined() )
        for ( auto dirEntry = theDirectory.Get(); dirEntry.defined(); dirEntry = theDirectory.Get() )
        {
          auto  filePath = mtc::strprintf( "%s%s", dirEntry.folder(), dirEntry.string() );
          auto  pointPos = filePath.find_first_of( '.', asteriskOffs );
          auto  toSuffix = filePath.substr( asteriskOffs, pointPos - asteriskOffs );

          sortedSuffix.push_back( std::move( toSuffix) );
        }
    }

    return !theInstances.empty() ? new SourceList( std::move( theInstances ) ) : nullptr;
  }

  auto  Storage::CreateStore() -> mtc::api<IIndexStore>
  {
    return CreateSink( policies );
  }

  auto  Open( const StoragePolicies& policies ) -> mtc::api<IStorage>
  {
    return new Storage( policies );
  }

}}}
