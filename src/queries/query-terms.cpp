# include "../../queries/builder.hpp"
# include "query-tools.hpp"

namespace DelphiX {
namespace queries {

  void  LoadQueryTerms( mtc::zmap& terms, const mtc::zval& query );

  void  LoadQueryTerms( mtc::zmap& terms, const mtc::array_zval& query )
  {
    for ( auto& next: query )
      LoadQueryTerms( terms, next );
  }

  void  LoadQueryTerms( mtc::zmap& terms, const mtc::zval& query )
  {
    auto  op = GetOperator( query );

    if ( op == "word" )
      return (void)(terms[op.GetString()] = mtc::zmap{ { "count", uint32_t(0) } });

    if ( op == "&&" || op == "||" || op == "quote" || op == "order" || op == "fuzzy" )
      return LoadQueryTerms( terms, op.GetVector() );

    if ( op == "cover" || op == "match" || op == "limit" )
    {
      auto  pquery = op.GetStruct().get( "query" );

      return pquery != nullptr ? LoadQueryTerms( terms, *pquery ) :
        throw std::invalid_argument( "missing 'query' as limited object" );
    }

    throw std::logic_error( mtc::strprintf( "unknown operator '%s' @"
      __FILE__ ":" LINE_STRING, (const char*)op ) );
  }

  auto  LoadQueryTerms( const mtc::zval& query ) -> mtc::zmap
  {
    auto  terms = mtc::zmap{
      { "collection-size", uint32_t(0) },
      { "terms-range-map", mtc::zmap{} } };

    return LoadQueryTerms( *terms.get_zmap( "terms-range-map" ), query ), terms;
  }

  auto  RankQueryTerms(
    const mtc::zmap&                terms,
    const mtc::api<IContentsIndex>& index,
    const context::Processor&       lproc ) -> mtc::zmap
  {
    auto  ntotal = index->GetMaxIndex();
    auto  zterms = mtc::zmap{
      { "collection-size", ntotal },
      { "terms-range-map", terms.get_zmap( "terms-range-map", {} ) } };

    if ( ntotal == 0 )
      return zterms;

    for ( auto& next: *zterms.get_zmap( "terms-range-map" ) )
      if ( next.first.is_widestr() && next.second.get_type() == mtc::zval::z_zmap )
      {
        auto  lexset = lproc.Lemmatize( next.first.to_widestr() );
        auto  weight = 1.0;
        auto  ncount = uint32_t{};

        for ( auto& lexeme: lexset )
        {
          auto  lstats = index->GetKeyStats( lexeme );

          if ( lstats.nCount != 0 )
            weight *= (1.0 - 1.0 * lstats.nCount / ntotal);
        }

      // get approximated count
        next.second.get_zmap()->set_word32( "count", ncount = ntotal * (1.0 - weight) );

      // get term range as idf with transformations
        if ( ncount != 0 )
          next.second.get_zmap()->set_double( "range", log( (1 + ntotal) / ncount ) / log( 1+ ntotal) );
        else
          next.second.get_zmap()->set_double( "range", 0.0 );
      }
        else
      {
        throw std::invalid_argument( "terms are expected to be widestrings pointing to structs @"
          __FILE__ ":" LINE_STRING );
      }

    return terms;
  }

}}
