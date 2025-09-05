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

    if ( op == "wildcard" )
      return (void)(terms[widechar('{') + op.GetString() + widechar('}')] = mtc::zmap{ { "count", uint32_t(0) } });

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

  auto  RankLexeme(
    const mtc::widestr&             token,
    const mtc::api<IContentsIndex>& index,
    const context::Processor&       lproc ) -> mtc::zmap
  {
    auto  lexset = lproc.Lemmatize( token );
    auto  weight = 1.0;
    auto  ntotal = index->GetMaxIndex();
    auto  ncount = uint32_t{};

    for ( auto& lexeme: lexset )
    {
      auto  lstats = index->GetKeyStats( lexeme );

      if ( lstats.nCount != 0 )
        weight *= (1.0 - 1.0 * lstats.nCount / ntotal);
    }

    // get approximated count
    return {
      { "count", ncount = ntotal * (1.0 - weight) },
      { "range", double(ncount != 0 ? log( (1.0 + ntotal) / ncount ) / log(1.0 + ntotal) : 0.0) } };
  }

  auto  RankJocker(
    const mtc::widestr&             token,
    const mtc::api<IContentsIndex>& index,
    const context::Processor&       lproc ) -> mtc::zmap
  {
    auto  keyTempl = context::Key( 0xff, token );
    auto  iterator = index->ListContents( keyTempl );
    auto  docTotal = index->GetMaxIndex() * 1.0;
    auto  negRange = 1.0;
    auto  keyCount = uint32_t{};

  // get additional probability for terms
    for ( auto next = iterator->Curr(); next.size() != 0 && negRange >= 0.01; next = iterator->Next() )
      negRange *= 1.0 - index->GetKeyStats( next ).nCount / docTotal;

    // get approximated count
    return {
      { "count", keyCount = uint32_t(docTotal * (1.0 - negRange)) },
      { "range", double(keyCount != 0 ? log( (1.0 + docTotal) / keyCount ) / log(1.0 + docTotal) : 0.0) } };
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
        auto  keystr = mtc::widestr( next.first.to_widestr() );

        if ( keystr.length() > 2 && keystr.front() == '{' && keystr.back() == '}' )
          next.second = RankJocker( keystr.substr( 1, keystr.length() - 2 ), index, lproc );
        else
          next.second = RankLexeme( keystr, index, lproc );
      }
        else
      {
        throw std::invalid_argument( "terms are expected to be widestrings pointing to structs @"
          __FILE__ ":" LINE_STRING );
      }

    return terms;
  }

}}
