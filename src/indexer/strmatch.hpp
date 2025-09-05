# if !defined( __DelphiX_src_indexer_strmatch_hpp__ )
# define __DelphiX_src_indexer_strmatch_hpp__
# include <mtc/radix-tree.hpp>
# include <string>

namespace DelphiX {
namespace indexer {

  int   strmatch( const char* sbeg, const char* send, const char* mbeg, const char* mend );
  int   strmatch( const std::string& str, const mtc::radix::key& match );

}}
# endif   // !__DelphiX_src_indexer_strmatch_hpp__
