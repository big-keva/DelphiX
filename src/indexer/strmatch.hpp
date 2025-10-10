# if !defined( __DelphiX_src_indexer_strmatch_hpp__ )
# define __DelphiX_src_indexer_strmatch_hpp__
# include <mtc/radix-tree.hpp>
# include <string>

namespace DelphiX {
namespace indexer {

  int   strmatch( const mtc::radix::key&, const std::string& tpl );

}}

# endif   // !__DelphiX_src_indexer_strmatch_hpp__
