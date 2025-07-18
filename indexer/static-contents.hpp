# if !defined( __DelphiX_indexer_static_contents_hpp__ )
# define __DelphiX_indexer_static_contents_hpp__
# include "contents.hpp"

namespace DelphiX {
namespace indexer {
namespace static_ {

  struct Contents
  {
    auto  Create( mtc::api<IStorage::ISerialized> ) -> mtc::api<IContentsIndex>;
  };

}}}

# endif   // !__DelphiX_indexer_static_contents_hpp__
