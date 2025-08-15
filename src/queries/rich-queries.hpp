# if !defined( __DelphiX_src_queries_rich_queries_hpp__ )
# define __DelphiX_src_queries_rich_queries_hpp__
# include "../../context/processor.hpp"
# include "../../contents.hpp"
# include "../../queries.hpp"
# include "../../fields.hpp"
# include <mtc/zmap.h>

namespace DelphiX {
namespace queries {

  auto  GetRichQuery(
    const mtc::zmap&                query,
    const mtc::zmap&                terms,
    const mtc::api<IContentsIndex>& index,
    const context::Processor&       lproc,
    const FieldHandler&             fdset ) -> mtc::api<IQuery>;

}}

# endif   // !__DelphiX_src_queries_rich_queries_hpp__
