# if !defined( __DelphiX_src_context_base_queries_hpp__ )
# define __DelphiX_src_context_base_queries_hpp__
# include "../../queries.hpp"
# include "../../contents.hpp"
# include <vector>

namespace DelphiX {
namespace queries {

  struct SimpleQuery: IQuery
  {
    SimpleQuery( const mtc::api<IContentsIndex::IEntities>& ent ):
      entities( ent ) {}

  // IQuery overridables
    uint32_t      SearchDoc( uint32_t ) override;

  protected:
    mtc::api<IContentsIndex::IEntities>   entities;
    IContentsIndex::IEntities::Reference  entryRef = { 0, {} };

  };

  struct SpreadQuery: IQuery
  {
    void  AddQuery( mtc::api<IQuery>, double );

  protected:
    struct SubQuery;

    std::vector<SubQuery> spread;
    uint32_t              uDocId = 0;
    double                quorum = 1.0;

  };

  class GetMatchAll: public SpreadQuery
  {
    uint32_t      SearchDoc( uint32_t ) override;
  };

  class GetMatchAny: public SpreadQuery
  {
    uint32_t      SearchDoc( uint32_t ) override;
  };

  class GetByQuorum: public SpreadQuery
  {
    uint32_t      SearchDoc( uint32_t ) override;
  };

  struct SpreadQuery::SubQuery
  {
    double              fRange;    // ранг подзапроса
    double              fLeast;    // ранг подзапросов правее
    unsigned            uOrder;    // порядковый номер подзапроса во фразе
    mtc::api<IQuery>    pQuery;
    unsigned            uDocId = 0;

    uint32_t  SearchDoc( uint32_t tofind )
      {  return tofind == uDocId ? uDocId : uDocId = pQuery->SearchDoc( tofind );  }
  };

}}

# endif   // !__DelphiX_src_context_base_queries_hpp__
