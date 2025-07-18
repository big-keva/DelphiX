# if !defined( __DelphiX_src_indexer_commit_contents_hxx__ )
# define __DelphiX_src_indexer_commit_contents_hxx__
# include "../../contents.hpp"
# include "notify-events.hpp"

namespace DelphiX {
namespace indexer {
namespace commit {

  struct Contents
  {
    auto  Create( mtc::api<IContentsIndex>, Notify::Func ) -> mtc::api<IContentsIndex>;
  };

}}}

# endif // !__DelphiX_src_indexer_commit_contents_hxx__
