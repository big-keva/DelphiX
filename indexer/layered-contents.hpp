# if !defined( __DelphiX_indexer_layered_contents_hpp__ )
# define __DelphiX_indexer_layered_contents_hpp__
# include "../contents.hpp"
# include "dynamic-contents.hpp"
# include <functional>

namespace DelphiX {
namespace indexer {
namespace layered {

  class Index
  {
    mtc::api<IStorage>    contentsStorage;
    dynamic::Settings     dynamicSettings;
    std::chrono::seconds  runMonitorDelay = std::chrono::seconds( 0 );

  public:
    auto  Set( const dynamic::Settings& ) -> Index&;
    auto  Set( mtc::api<IStorage> ) -> Index&;
    auto  Create() -> mtc::api<IContentsIndex>;

    static  auto  Create( const mtc::api<IContentsIndex>*, size_t ) -> mtc::api<IContentsIndex>;
    static  auto  Create( const std::vector<mtc::api<IContentsIndex>>& ) -> mtc::api<IContentsIndex>;

  };

}}}

# endif   // !__DelphiX_indexer_layered_contents_hpp__
