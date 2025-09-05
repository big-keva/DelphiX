# include "base-queries.hpp"
//# include "../../context/x-queries.hpp"

namespace DelphiX {
namespace context {

  class ZeroSimpleQuery final: public SimpleQuery
  {
    using SimpleQuery::SimpleQuery;

    const Tuples* GetTuples( uint32_t ) override;

  public:
    implement_lifetime_control

  protected:
    Tuples::BM25Ref bmEntry{ unsigned(-1), 1, 0.0 };
    Tuples          bmTuple{ Tuples::Mode::BM25, 0, 1, &bmEntry };
  };

  class ZeroConjunction final: public Conjunction
  {
    const Tuples* GetTuples( uint32_t ) override;

  public:
    implement_lifetime_control

  protected:
    Tuples                        bmTuple{ Tuples::Mode::BM25, 0, 0, nullptr };
    std::vector<Tuples::BM25Ref>  aTuples;
  };

  class ZeroDisjunction final: public Disjunction
  {
    const Tuples* GetTuples( uint32_t ) override;

  protected:
    Tuples::BM25Ref bmEntry{ 0, 1, 0.0 };
    Tuples          bmTuple{ Tuples::Mode::BM25, 0, 1, &bmEntry };

    implement_lifetime_control
  };

  class ZeroQuorumMatch final: public QuorumMatch
  {
    const Tuples* GetTuples( uint32_t ) override;

  protected:
    Tuples                        bmTuple{ Tuples::Mode::BM25, 0, 1, nullptr };
    std::vector<Tuples::BM25Ref>  aTuples;

    implement_lifetime_control
  };
# if 0
  class StructParser
  {
    const mtc::api<IContentsIndex>& contentsIndex;
    const Processor&                langProcessor;
    mtc::zmap                       termsRankings;
    const FieldHandler*             fieldSettings;

    struct Query
    {
      mtc::api<IQuery>  query;
      double            range;
    };

    typedef Query (StructParser::*FnSubQuery)( const mtc::zmap& ) const;

    static  auto  GetMaxWeight( const std::vector<Query>& queries ) -> double;
    static  auto  GetMinWeight( const std::vector<Query>& queries ) -> double;
    static  auto  GetSumWeight( const std::vector<Query>& queries ) -> double;

  public:
    StructParser( const mtc::api<IContentsIndex>& x, const Processor& p, const mtc::zmap& t, const FieldHandler* f ):
      contentsIndex( x ),
      langProcessor( p ),
      termsRankings( t ),
      fieldSettings( f )  {}

    auto  BuildQuery( const mtc::zmap& ) -> Query;
    auto  BuildTerms( const mtc::zmap& ) -> mtc::zmap;

  protected:
    static  auto  GetCommand( const mtc::zmap& ) -> mtc::zmap::const_iterator;
    static  bool  IsOperator( mtc::zmap::const_iterator, const std::initializer_list<const char*>& );
    static  auto  GetArgList( const mtc::zmap::const_iterator& ) -> const mtc::array_zmap&;
    static  auto  FetchTerms( const mtc::zmap& ) -> mtc::zmap;

  protected:

    template <class Target, bool strict, class Weight>
    auto  CreateOperator( const mtc::array_zmap&, FnSubQuery, Weight weight ) const -> Query;

    auto  CreateTextQuery( const mtc::widestr& ) const -> Query;
    auto  CreateTextQuery( const mtc::zval& ) const -> Query;
    auto  CreateSingleWord( const Slice<const Lexeme>& ) const -> Query;
    auto  CreateSimpleTerm( const Lexeme& ) const -> Query;

  };

  // ZeroSimpleQuery implementation

  auto  ZeroSimpleQuery::GetTuples( uint32_t entity ) -> const Tuples*
  {
    return entity == entryRef.uEntity ? &bmTuple : nullptr;
  }

  // ZeroConjunction implementation

  auto  ZeroConjunction::GetTuples( uint32_t entity ) -> const Tuples*
  {
    auto          output = aTuples.data();
    const Tuples* tuples;

    for ( auto& next: subSet )
      if ( next.uDocId == entity && (tuples = next.pQuery->GetTuples( entity )) != nullptr )
        *output++ = *tuples->pNodes;

    return (bmTuple.nItems = output - aTuples.data()) != 0 ? &bmTuple : nullptr;
  }

  // ZeroDisjunction implementation

  auto  ZeroDisjunction::GetTuples( uint32_t entity ) -> const Tuples*
  {
    const Tuples* tuples;
    double        weight = 1.0;

    for ( auto& next: subSet )
      if ( next.uDocId == entity && (tuples = next.pQuery->GetTuples( entity )) != nullptr )
        weight *= 1.0 - tuples->pNodes->dblIDF;

    return (bmEntry.dblIDF = 1.0 - weight) >= 1e-5 ? &bmTuple : nullptr;
  }

  // ZeroQuorumMatch implementation

  auto  ZeroQuorumMatch::GetTuples( uint32_t entity ) -> const Tuples*
  {
    auto          output = aTuples.data();
    const Tuples* tuples;

    for ( auto& next: subSet )
      if ( next.uDocId == entity && (tuples = next.pQuery->GetTuples( entity )) != nullptr )
        *output++ = *tuples->pNodes;

    return (bmTuple.nItems = output - aTuples.data()) != 0 ? &bmTuple : nullptr;
  }

  // StructParser implementation

  auto  GetMiniQuery(
    const mtc::api<IContentsIndex>& index,
    const Processor&                lproc,
    const mtc::zmap&                query,
    const FieldHandler*             fdset ) -> mtc::api<IQuery>
  {
    auto  aquery = query.get_zmap( "query", {} );
    auto  aterms = query.get_zmap( "terms", {} );

    return StructParser( index, lproc, aterms, fdset ).BuildQuery( aquery ).query;
  }

  auto  StructParser::BuildQuery( const mtc::zmap& query ) -> Query
  {
    auto  command = GetCommand( query );

    if ( IsOperator( command, { "&", "&&", "AND", "and", "|", "||", "OR", "or", "!", "NOT", "not", "phrase", "sequence" } ) )
    {
      auto& argset = GetArgList( command );

      if ( IsOperator( command, { "&", "&&", "AND", "and", "phrase", "sequence" } ) )
        return CreateOperator<ZeroConjunction, true>( argset, &StructParser::BuildQuery, GetMaxWeight );
      if ( IsOperator( command, { "|", "||", "OR", "or" } ) )
        return CreateOperator<ZeroDisjunction, true>( argset, &StructParser::BuildQuery, GetSumWeight );
//      if ( IsOperator( command, { "!", "NOT", "not" } ) )
//        return CreateExclusion( argset );
      throw std::logic_error( "we forgot to support the command" );
    }
    if ( IsOperator( command, { "text" } ) )
      return CreateTextQuery( command->second );
  }
/*
  auto  StructParser::BuildTerms( const mtc::zmap& query ) const -> mtc::zmap
  {
  }
  */
  auto  StructParser::FetchTerms( const mtc::zmap& query ) -> mtc::zmap
  {
    auto  command = GetCommand( query );

    if ( IsOperator( command, { "&", "&&", "AND", "and", "|", "||", "OR", "or", "!", "NOT", "not", "phrase", "sequence" } ) )
    {
      auto& argset = GetArgList( command );

      if ( IsOperator( command, { "&", "&&", "AND", "and", "phrase", "sequence" } ) )
        return CreateOperator<ZeroConjunction, true>( argset, &StructParser::BuildQuery, GetMaxWeight );
      if ( IsOperator( command, { "|", "||", "OR", "or" } ) )
        return CreateOperator<ZeroDisjunction, true>( argset, &StructParser::BuildQuery, GetSumWeight );
      //      if ( IsOperator( command, { "!", "NOT", "not" } ) )
      //        return CreateExclusion( argset );
      throw std::logic_error( "we forgot to support the command" );
    }
    if ( IsOperator( command, { "text" } ) )
      return CreateTextQuery( command->second );
  }
# if 0
  auto  StructParser::GetText( const mtc::zval& ztext ) const -> Query
  {
    switch ( ztext.get_type() )
    {
      case mtc::zval::z_charstr:
        return GetText( codepages::mbcstowide( codepages::codepage_utf8, *ztext.get_charstr() ) );
      case mtc::zval::z_widestr:
        return GetText( *ztext.get_widestr() );
      default:
        throw std::invalid_argument( "'text' query is expected to be string" );
    }
  }

  auto  StructParser::GetText( const mtc::widestr& ztext ) const -> Query
  {
    auto  image = lpr.MakeImage( textAPI::Document{ ztext } );
    auto  query = mtc::api<ZeroQuorumMatch>();
    auto  words = std::vector<Query>();
    auto  qnext = Query();

  // add all the words to words collector
    for ( auto& next: image.GetLemmas() )
      if ( (qnext = GetWord( next )).query != nullptr )
        words.push_back( qnext );

    if ( words.empty() )
      return {};

    if ( words.size() == 1 )
      return words.front();

    query = new ZeroQuorumMatch();

    for ( auto& next: words )
      query->AddSubquery( next.query, next.range );

    return { query.ptr(), words.front().range };
  }

  auto  StructParser::GetWord( const Slice<const Lexeme>& word ) const -> Query
  {
    auto  terms = std::vector<Query>();
    auto  alloc = Query();
    auto  query = mtc::api<Disjunction>();

    for ( auto& next: word )
      if ( (alloc = GetTerm( next )).query != nullptr )
        terms.push_back( alloc );

    if ( terms.empty() )
      return {};

    if ( terms.size() == 1 )
      return terms.front();

    query = new ZeroDisjunction();

    for ( auto& next: terms )
      query->AddSubquery( next.query, next.range );

    return { query.ptr(), terms.front().range };
  }

  auto  StructParser::GetTerm( const Lexeme& term ) const -> Query
  {
    auto  pblock = ctx->GetKeyBlock( term );

    if ( pblock != nullptr )
      return { new ZeroSimpleQuery( pblock ), 1.0 };
    return { nullptr, 0.0 };
  }

  auto  GetTerms( mtc::zmap& terms, const mtc::zmap& query ) -> mtc::zmap&
  {
    auto  opcode = GetOperator( query );

    if ( opcode == query.end() )
      return terms;

    /*
     * logical operators handle arrays of arguments, each of arguments has to be another
     * logical expression
     */
    if ( IsOperator( opcode, { "&", "&&", "AND", "and", "|", "||", "OR", "or", "!", "NOT", "not" } ) )
    {
      auto  argset = opcode->second.get_array_zmap();

      if ( argset == nullptr )
      {
        throw std::invalid_argument( mtc::strprintf( "operator '%s' has to point to the array of structure arguments",
          opcode->first.to_charstr() ) );
      }
      for ( auto& next: *argset )
        GetTerms( terms, next );
    }
    return terms;
  }

  auto  GetTerms( const mtc::zmap& query ) -> mtc::zmap
  {
    mtc::zmap terms;

    return GetTerms( terms, query );
  }

  auto  BuildQuery( const mtc::zmap& query, const mtc::zmap& terms ) -> mtc::api<IQuery>
  {

  }
# endif

  auto StructParser::GetMaxWeight( const std::vector<Query>& queries ) -> double
  {
    auto  weight = 0.0;

    for ( auto& query: queries )
      weight = std::max( weight, query.range );

    return weight;
  }

  auto StructParser::GetMinWeight( const std::vector<Query>& queries ) -> double
  {
    auto  weight = 1.0;

    for ( auto& query: queries )
      weight = std::min( weight, query.range );

    return weight;
  }

  auto StructParser::GetSumWeight( const std::vector<Query>& queries ) -> double
  {
    auto  weight = 1.0;

    for ( auto& query: queries )
      weight *= (1.0 - query.range);

    return 1.0 - weight;
  }

  auto  StructParser::GetCommand( const mtc::zmap& query ) const -> mtc::zmap::const_iterator
  {
    auto  nextIt = query.begin();

    if ( nextIt == query.end() )
      throw std::invalid_argument( "query does not have an operator" );
    if ( query.size() != 1 )
      throw std::invalid_argument( "query has more than one entry" );
    return nextIt;
  }

  bool  StructParser::IsOperator( mtc::zmap::const_iterator op, const std::initializer_list<const char*>& vs ) const
  {
    if ( op->first.is_charstr() )
      for ( auto& next: vs )
        if ( strcmp( next, op->first.to_charstr() ) ) return true;

    return false;
  }

  auto  StructParser::GetOperatorArgs( const mtc::zmap::const_iterator& cmd ) const -> const mtc::array_zmap&
  {
    auto  argset = cmd->second.get_array_zmap();

    if ( argset == nullptr || argset->empty() )
      throw std::invalid_argument( mtc::strprintf( "operator '%s' has to point to the array of structure arguments",
        cmd->first.to_charstr() ) );
    return *argset;
  }

  template <class Target, bool strict, class Weight>
  auto  StructParser::CreateOperator( const mtc::array_zmap& args, FnSubQuery fnCreate, Weight weight ) const -> Query
  {
    auto  subset = std::vector<Query>{};
    auto  output = mtc::api<SubquerySet>();
    auto  subarg = Query();

    if ( args.empty() )
      throw std::logic_error( "condition for call with empty arguments was not checked" );

    for ( auto& arg: args )
      if ( (subarg = (this->*fnCreate)( arg )).query != nullptr )  subset.emplace_back( subarg );
        else if ( strict ) return { nullptr, 0.0 };

    if ( subset.empty() )
      return { nullptr, 0.0 };

    if ( subset.size() == 1 )
      return subset.front();

    output = new Target();

    for ( auto& sub: subset )
      output->AddSubquery( sub.query, sub.range );

    return { output.ptr(), weight( subset ) };
  }

  auto  StructParser::CreateTextQuery( const mtc::widestr& str ) const -> Query
  {
    auto  wimage = langProcessor.MakeImage( textAPI::Document{ str }, fieldSettings );
    auto  awords = std::vector<Query>();
    auto  weight = 0.0;
    auto  output = mtc::api<QuorumMatch>();

    for ( auto& word: wimage.GetLemmas() )
    {
      auto  subreq = CreateSingleWord( word );

      if ( subreq.query == nullptr )
        continue;
      awords.emplace_back( subreq );
        weight += subreq.range;
    }

    if ( awords.empty() )
      return { nullptr, 0.0 };

    if ( awords.size() == 1 )
      return awords.front();

    output = new ZeroQuorumMatch();

    for ( auto& word: awords )
      output->AddSubquery( word.query, word.range );

    return { output.ptr(), weight / awords.size() };
  }

  auto  StructParser::CreateTextQuery( const mtc::zval& val ) const -> Query
  {
    switch ( val.get_type() )
    {
      case mtc::zval::z_charstr:
        return CreateTextQuery( codepages::widetombcs( codepages::codepage_utf8, *val.get_widestr() ) );
      case mtc::zval::z_widestr:
        return CreateTextQuery( *val.get_widestr() );
      default:
        throw std::invalid_argument( "'text', 'phrase' and 'sequence' operators expect argument as string" );
    }
  }

  auto  StructParser::CreateSingleWord( const Slice<const Lexeme>& lexemes  ) const -> Query
  {
    auto  aterms = std::vector<Query>();
    auto  weight = 1.0;
    auto  output = mtc::api<Disjunction>();

    for ( auto& lex: lexemes )
    {
      auto  subarg = CreateSimpleTerm( lex );

      if ( subarg.query == nullptr )
        continue;
      weight *= 1.0 - subarg.range;
        aterms.emplace_back( subarg );
    }

    if ( aterms.empty() )
      return { nullptr, 0.0 };

    if ( aterms.size() == 1 )
      return aterms.front();

    output = new ZeroDisjunction();

    for ( auto& term: aterms )
      output->AddSubquery( term.query, term.range );

    return { output.ptr(), 1.0 - weight };
  }

  auto  StructParser::CreateSimpleTerm( const Lexeme& lex ) const -> Query
  {
/*    auto  pstats = termsRankings.get_zmap( )
    auto  output = mtc::api<Disjunction>();

    for ( auto& lex: lexemes )
    {
      auto  subarg = CreateSimpleTerm( lex );

      if ( subarg.query == nullptr )
        continue;
      weight *= 1.0 - subarg.range;
        aterms.emplace_back( subarg );
    }

    if ( aterms.empty() )
      return { nullptr, 0.0 };

    if ( aterms.size() == 1 )
      return aterms.front();

    output = new ZeroDisjunction();

    for ( auto& term: aterms )
      output->AddSubquery( term.query, term.range );

    return { output.ptr(), 1.0 - weight };*/
  }
# endif   // 0
}}
