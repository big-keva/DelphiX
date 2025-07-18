# include "../../storage/posix-fs.hpp"
# include <mtc/wcsstr.h>
# include <stdexcept>
# include <vector>

namespace DelphiX {
namespace storage {
namespace posixFS {

  struct StoragePolicies::Impl: public std::vector<Policy>
  {
    using std::vector<Policy>::vector;

  public:
    Impl( bool isInst = false ) {  isInstance = isInst;  }

  public:
    bool              isInstance;
    std::atomic_long  referCount = 1;
  };

  // Policy implementation

  auto  Policy::GetFilePath( Unit to, const char* stamp ) const -> std::string
  {
    if ( (unit & to) != to || (unit & to) == 0 )
      throw std::invalid_argument( "Invalid index unit id requested" );

    return mtc::strprintf( path.c_str(), stamp )
      + "." + StoragePolicies::GetSuffix( Unit(unit & to) );
  }

  // StoragePolicies implementation

  StoragePolicies::StoragePolicies( std::initializer_list<Policy> policies ):
    StoragePolicies( policies.begin(), policies.end() ) {}

  StoragePolicies::StoragePolicies( const Policy* policies, size_t n ):
    StoragePolicies( policies, policies + n ) {}

  StoragePolicies::StoragePolicies( StoragePolicies&& policies ):
    impl( policies.impl )
  {
    policies.impl = nullptr;
  }

  StoragePolicies::StoragePolicies( const StoragePolicies& policies ):
    impl( policies.impl )
  {
    if ( impl != nullptr )
      ++impl->referCount;
  }

  StoragePolicies::~StoragePolicies()
  {
    if ( impl != nullptr && --impl->referCount == 0 )
      delete impl;
  }

  auto  StoragePolicies::operator=( const StoragePolicies& policies ) -> StoragePolicies&
  {
    if ( impl != nullptr && --impl->referCount == 0 )
      delete impl;
    if ( (impl = policies.impl) != nullptr )
      ++impl->referCount;
    return *this;
  }

  auto  StoragePolicies::GetInstance( const char* stamp ) const -> StoragePolicies
  {
    StoragePolicies policies;

    if ( stamp == nullptr || *stamp == '\0' )
      throw std::invalid_argument( "Invalid stamp string provided" );

    if ( impl != nullptr )
    {
      policies.impl = new Impl( true );

      for ( auto& policy: *impl )
        policies.impl->push_back( { policy.unit, policy.mode, mtc::strprintf( policy.path.c_str(), stamp ) } );
    }

    return policies;
  }

  auto  StoragePolicies::GetInstance( const std::string& stamp ) const -> StoragePolicies
  {
    return GetInstance( stamp.c_str() );
  }

  bool StoragePolicies::IsInstance() const
  {
    return impl != nullptr ? impl->isInstance : false;
  }

  auto  StoragePolicies::Open( const std::string& generic_path ) -> StoragePolicies
  {
    return StoragePolicies( { { Unit( (status << 1) - 1 ), memory_mapped, generic_path } } );
  }

  auto  StoragePolicies::OpenInstance( const std::string& instance_path ) -> StoragePolicies
  {
    StoragePolicies policies;

    (policies.impl = new Impl( true ))
      ->push_back( { Unit( (status << 1) - 1 ), memory_mapped, instance_path } );

    return policies;
  }

  auto  StoragePolicies::AddPolicy( const Policy& policy ) -> StoragePolicies&
  {
  // check valid policy path passed
    if ( policy.path.empty() )
      throw std::invalid_argument( "Invalid (empty) policy path provided" );

    if ( strchr( "\\/.", policy.path[policy.path.length() - 1] ) != nullptr )
      throw std::invalid_argument( "Policy path does not contain the generic index name" );

  // TODO: check invalid directory
    if ( impl == nullptr )
      impl = new Impl();

    impl->push_back( { policy.unit, policy.mode, std::string( policy.path ) + ".%s" } );

    return *this;
  }

  auto  StoragePolicies::AddPolicy( Unit unit, Mode mode, std::string_view path ) -> StoragePolicies&
  {
    return AddPolicy( { unit, mode, std::string( path ).c_str() } );
  }

  auto  StoragePolicies::GetPolicy( Unit unit ) const -> const Policy*
  {
    if ( impl != nullptr )
      for ( auto& next: *impl )
        if ( (next.unit & unit) != 0 )
          return &next;
    return nullptr;
  }

  auto  StoragePolicies::GetSuffix( Unit unit ) -> const char*
  {
    switch ( unit )
    {
      case entities:    return "entities";
      case attributes:  return "attributes";
      case contents:    return "contents";
      case blocks:      return "blocks";
      case images:      return "images";
      case status:      return "status";
      default:          throw std::invalid_argument( "invalid Unit id" );
    }
  }

}}}