# if !defined( __DelphiX_queries_builder_hpp__ )
# define __DelphiX_queries_builder_hpp__
# include "../context/processor.hpp"
# include "../contents.hpp"
# include "../queries.hpp"
# include <mtc/zmap.h>

namespace DelphiX {
namespace queries {

  auto  LoadQueryTerms(
    const mtc::zval&                query ) -> mtc::zmap;

  auto  RankQueryTerms(
    const mtc::zmap&                terms,
    const mtc::api<IContentsIndex>& index,
    const context::Processor&       lproc ) -> mtc::zmap;

  auto  BuildRichQuery(
    const mtc::zval&                query,
    const mtc::zmap&                terms,
    const mtc::api<IContentsIndex>& index,
    const context::Processor&       lproc,
    const FieldHandler&             fdset ) -> mtc::api<IQuery>;

  auto  BuildBM25Query(
    const mtc::zval&                query,
    const mtc::zmap&                terms,
    const mtc::api<IContentsIndex>& index,
    const context::Processor&       lproc,
    const FieldHandler&             fdset ) -> mtc::api<IQuery>;

  auto  BuildMiniQuery(
    const mtc::zval&                query,
    const mtc::zmap&                terms,
    const mtc::api<IContentsIndex>& index,
    const context::Processor&       lproc,
    const FieldHandler&             fdset ) -> mtc::api<IQuery>;

}}

# endif   // !__DelphiX_queries_builder_hpp__
