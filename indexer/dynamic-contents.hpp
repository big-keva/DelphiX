# if !defined( __DelphiX_indexer_dynamic_contents_hpp__ )
# define __DelphiX_indexer_dynamic_contents_hpp__
# include "../contents.hpp"

namespace DelphiX {
namespace indexer {
namespace dynamic {

  struct Settings
  {
    uint32_t  maxEntities = 2000;                 /* */
    uint32_t  maxAllocate = 256 * 1024 * 1024;    /* 256 meg */

  public:
    auto  SetMaxEntities( uint32_t value ) -> Settings& {  maxEntities = value; return *this;  }
    auto  SetMaxAllocate( uint32_t value ) -> Settings& {  maxAllocate = value; return *this;  }
  };

  class Index
  {
    using Storage = mtc::api<IStorage::IIndexStore>;

    Settings  openOptions;
    Storage   storageSink;

  public:
    auto  Set( const Settings& ) -> Index&;
    auto  Set( mtc::api<IStorage::IIndexStore> ) -> Index&;

  public:
    auto  Create() const -> mtc::api<IContentsIndex>;
  };

}}}

# endif   // !__DelphiX_indexer_dynamic_contents_hpp__
