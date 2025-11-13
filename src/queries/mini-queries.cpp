# include "../../queries/builder.hpp"
# include "query-tools.hpp"
# include "context/processor.hpp"
# include <mtc/bitset.h>

namespace DelphiX {
namespace queries {

  using IEntities = IContentsIndex::IEntities;
  using Reference = IEntities::Reference;
  using BM25Term  = Abstract::BM25Term;

 /*
  * MiniQueryBase обеспечивает синхронное продвижение по форматам вместе с координатами
  * и передаёт распакованные форматы альтернативному методу доступа ко вхождениям.
  */
  struct MiniQueryBase: IQuery
  {
    uint32_t  entityId = 0;                       // the found entity
    Abstract  abstract = {};

  public:
    auto  SetAbstract( const BM25Term* beg, const BM25Term* end ) -> Abstract&
    {
      return (abstract = { end != beg ? Abstract::BM25 : Abstract::None, 0U, {} })
        .factors = { beg, end }, abstract;
    }
  };

 /*
  * MiniQueryTerm - самый простой термин с одним координатным блоком, реализующий
  * поиск и ранжирование слова с одной лексемой
  */
  struct MiniQueryTerm final: MiniQueryBase
  {
    implement_lifetime_control

  // construction
    MiniQueryTerm( const mtc::api<IEntities>& blk, double idf ): MiniQueryBase(),
      entBlock( blk ),
      datatype( blk->Type() ),
      bm25Term{ 0, idf, 0, 0 } {}

  // overridables
    uint32_t  SearchDoc( uint32_t ) override;
    Abstract  GetTuples( uint32_t ) override;

  protected:
    mtc::api<IEntities> entBlock;
    const unsigned      datatype;
    Reference           docRefer;
    Abstract::BM25Term  bm25Term;

  };

 /*
  * MiniMultiTerm - коллекция их нескольких омонимов терминов, реализованная для
  * некоторой оптимизации вычислений - чтобы меньше было виртуальных вызовов и
  * больше использовался процессорный кэш
  */
  class MiniMultiTerm final: public MiniQueryBase
  {
    struct KeyBlock
    {
      mtc::api<IEntities> entBlock;           // coordinates
      const unsigned      datatype;
      const double        idfValue;
      Reference           docRefer = { 0, { nullptr, 0 } };   // entity reference set

    // construction
      KeyBlock( const mtc::api<IEntities>& blk, double idf ):
        entBlock( blk ),
        datatype( blk->Type() ),
        idfValue( idf ){}
    };

    implement_lifetime_control

  // construction
    MiniMultiTerm( std::vector<std::pair<mtc::api<IEntities>, double>>& );

  // IQuery overridables
    uint32_t  SearchDoc( uint32_t ) override;
    Abstract  GetTuples( uint32_t ) override;

  protected:
    std::vector<KeyBlock>           blockSet;
    BM25Term                        bm25term;

  };

  class MiniQueryArgs: public MiniQueryBase
  {
  public:
    void   AddQueryNode( mtc::api<MiniQueryBase>, double );
    auto   StrictSearch( uint32_t ) -> uint32_t;

  protected:
    struct SubQuery
    {
      mtc::api<MiniQueryBase> subQuery;
      double                  keyRange;
      double                  leastSum = 0.0;
      unsigned                docFound = 0;
      Abstract                abstract = {};

    // methods
      auto  SearchDoc( uint32_t id ) -> uint32_t
      {
        if ( docFound >= id )
          return docFound;
        return abstract = {}, docFound = subQuery->SearchDoc( id );
      }
      auto  GetTuples( uint32_t id ) -> const Abstract&
      {
        return abstract.dwMode == abstract.None && docFound == id ?
          abstract = subQuery->GetTuples( id ) : abstract;
      }
    };

  protected:
    std::vector<SubQuery> querySet;
    BM25Term              termList[0x200];

  };

  class MiniQueryAll final: public MiniQueryArgs
  {
    using MiniQueryArgs::MiniQueryArgs;

    implement_lifetime_control

    // overridables
    uint32_t  SearchDoc( uint32_t id ) override  {  return StrictSearch( id );  }
    Abstract  GetTuples( uint32_t id ) override;

  };

  class MiniQueryFuzzy final: public MiniQueryArgs
  {
    using MiniQueryArgs::MiniQueryArgs;

    implement_lifetime_control

    // construction
    MiniQueryFuzzy( double quo ): MiniQueryArgs(),
      quorum( quo ) {}

    // overridables
    uint32_t  SearchDoc( uint32_t ) override;
    Abstract  GetTuples( uint32_t ) override;

  protected:
    double  quorum;

  };

  class MiniQueryAny final: public MiniQueryArgs
  {
    using MiniQueryArgs::MiniQueryArgs;

    implement_lifetime_control

    // overridables
    uint32_t  SearchDoc( uint32_t ) override;
    Abstract  GetTuples( uint32_t ) override;

  protected:
    std::vector<Abstract*>  selected;
  };

  // MiniQueryTerm implementation

 /*
  * Search next document in the list of entities
  */
  uint32_t  MiniQueryTerm::SearchDoc( uint32_t tofind )
  {
    if ( (tofind = std::max( std::max( entityId, tofind ), 1U )) == uint32_t(-1) )
      return entityId = uint32_t(-1);

    if ( entityId >= tofind )
      return entityId;

    return abstract = {}, entityId = (docRefer = entBlock->Find( tofind )).uEntity;
  }

 /*
  * For changed document id, unpack && return the entries and entry sets for given
  * format set
  */
  Abstract  MiniQueryTerm::GetTuples( uint32_t tofind )
  {
    if ( docRefer.uEntity == tofind && abstract.dwMode == Abstract::None )
      SetAbstract( &bm25Term, 1 + &bm25Term );
    return abstract;
  }

  // MiniMultiTerm implementation

  MiniMultiTerm::MiniMultiTerm( std::vector<std::pair<mtc::api<IEntities>, double>>& terms ):
    MiniQueryBase()
  {
    for ( auto& next: terms )
      blockSet.emplace_back( next.first, next.second );
  }

  uint32_t  MiniMultiTerm::SearchDoc( uint32_t tofind )
  {
    uint32_t  uFound = uint32_t(-1);;

    if ( (tofind = std::max( std::max( 1U, tofind ), entityId )) == uint32_t(-1) )
      return entityId = uint32_t(-1);

    if ( entityId >= tofind )
      return entityId;

    for ( auto& next: blockSet )
    {
      if ( next.docRefer.uEntity < tofind )
        next.docRefer = next.entBlock->Find( tofind );
      uFound = std::min( uFound, next.docRefer.uEntity );
    }

    return abstract = {}, entityId = uFound;
  }

  Abstract  MiniMultiTerm::GetTuples( uint32_t getdoc )
  {
    if ( getdoc == entityId && abstract.dwMode == Abstract::None )
    {
      bm25term = { 0, 0.0, 0, 1 };

      for ( auto& next: blockSet )
        if ( next.docRefer.uEntity == getdoc )
          bm25term.dblIDF = std::max( bm25term.dblIDF, next.idfValue );

      return SetAbstract( &bm25term, 1 + &bm25term );
    }
    return abstract;
  }

  // MiniQueryArgs implementation

  void  MiniQueryArgs::AddQueryNode( mtc::api<MiniQueryBase> query, double range )
  {
    double  rgsumm = 0.0;

    querySet.push_back( { query, range } );

    std::sort( querySet.begin(), querySet.end(), []( const SubQuery& s1, const SubQuery& s2 )
      {  return s1.keyRange > s2.keyRange; } );

    for ( auto& next : querySet )
      rgsumm += next.keyRange;

    for ( auto& next : querySet )
      next.leastSum = (rgsumm -= next.keyRange);
  }

  uint32_t  MiniQueryArgs::StrictSearch( uint32_t tofind )
  {
    if ( (tofind = std::max( tofind, entityId )) == uint32_t(-1) )
      return entityId = uint32_t(-1);

    if ( entityId != tofind )
      abstract = {};

    for ( size_t nstart = 0; ; )
    {
      auto  nfound = querySet[nstart].SearchDoc( tofind );

      if ( nfound == uint32_t(-1) )
        return entityId = uint32_t(-1);

      if ( nfound == tofind )
      {
        if ( ++nstart >= querySet.size() )
          return entityId = tofind;
      } else nstart = (nstart == 0) ? 1 : 0;

      tofind = nfound;
    }
  }

  // MiniQueryAll implementation

 /*
  * MiniQueryAll::GetChunks( ... )
  *
  * Fill abstract with term idf's for all the terms meet in subqueries
  */
  Abstract  MiniQueryAll::GetTuples( uint32_t udocid )
  {
    if ( abstract.dwMode == abstract.None )
    {
      auto  idfptr = termList;

      for ( auto& next: querySet )
        if ( next.GetTuples( udocid ).factors.size() != 0 )
        {
          while ( next.abstract.factors.beg != next.abstract.factors.end && idfptr != std::end(termList) )
            *idfptr++ = *next.abstract.factors.beg++;
        }
          else
        return abstract = {};

      return SetAbstract( termList, idfptr );
    }
    return abstract;
  }

  // MiniQueryFuzzy implementation

  uint32_t  MiniQueryFuzzy::SearchDoc( uint32_t tofind )
  {
    if ( (tofind = std::max( std::max( tofind, entityId ), 1U )) == uint32_t(-1) )
      return uint32_t(-1);

    for ( abstract = {}; ; )
    {
      double    weight = 0.0;
      uint32_t  ufound = uint32_t(-1);

      for ( size_t nstart = 0; nstart != querySet.size(); )
      {
        auto&    rquery = querySet[nstart];
        uint32_t nextId = rquery.SearchDoc( tofind );

        if ( nextId < ufound )
        {
          ufound = nextId;
          weight = 0;
        }

        if ( (weight += rquery.keyRange) + rquery.leastSum < quorum )
        {
          tofind = ufound + 1;
          ufound = uint32_t(-1);
          nstart = 0;
          weight = 0;
        } else ++nstart;
      }

    // check if is found or not
      if ( ufound == uint32_t(-1) || weight >= quorum )
        return entityId = ufound;
      tofind = ufound + 1;
    }
  }

  Abstract  MiniQueryFuzzy::GetTuples( uint32_t udocid )
  {
    if ( abstract.dwMode == abstract.None )
    {
      auto  idfptr = termList;
      auto  crange = 0.0;

      for ( auto& next: querySet )
        if ( next.GetTuples( udocid ).factors.size() != 0 )
        {
          while ( next.abstract.factors.beg != next.abstract.factors.end && idfptr != std::end(termList) )
            crange += (*idfptr++ = *next.abstract.factors.beg++).dblIDF;
        }

      return crange >= quorum ? SetAbstract( termList, idfptr ) : abstract = {};
    }
    return abstract;
  }

  // MiniQueryAny implementation

  uint32_t  MiniQueryAny::SearchDoc( uint32_t tofind )
  {
    uint32_t  uFound;

    if ( (tofind = std::max( tofind, entityId )) == uint32_t(-1) )
      return entityId = uint32_t(-1);

    if ( entityId != tofind )
      abstract = {};

    uFound = uint32_t(-1);

    for ( auto& next: querySet )
      uFound = std::min( uFound, next.SearchDoc( tofind ) );

    return entityId = uFound;
  }

  Abstract  MiniQueryAny::GetTuples( uint32_t udocid )
  {
    if ( abstract.dwMode == abstract.None )
    {
      auto  idfout = termList;

      for ( auto term = querySet.begin(); term != querySet.end() && idfout < std::end(termList); ++term )
        if ( term->GetTuples( udocid ).factors.size() != 0 )
        {
          for ( auto beg = term->abstract.factors.beg; beg < term->abstract.factors.end && idfout < std::end( termList ); )
            *idfout++ = *beg++;
        }

      return SetAbstract( termList, idfout );
    }
    return abstract;
  }

  // Query creation entry

  class MiniBuilder
  {
    const mtc::api<IContentsIndex>& index;
    const context::Processor&       lproc;
    const mtc::zmap&                terms;
    const uint32_t                  total;
    const mtc::zmap                 zstat;

  protected:
    struct SubQuery
    {
      mtc::api<MiniQueryBase> query;
      double                  range;
    };

  public:
    MiniBuilder( const mtc::api<IContentsIndex>& dx, const context::Processor& lp, const mtc::zmap& tm ):
      index( dx ),
      lproc( lp ),
      terms( tm ),
      total( std::max( terms.get_word32( "collection-size", 0 ), index->GetMaxIndex() ) ),
      zstat( tm.get_zmap( "terms-range-map", {} ) ) {}

    auto  BuildQuery( const mtc::zval& ) const -> SubQuery;

  template <bool Forced, class Output>
    auto  CreateArgs( mtc::api<Output>, const mtc::array_zval& ) const -> SubQuery;
    auto  CreateWord( const mtc::widestr& ) const -> SubQuery;
    auto  AsWildcard( const mtc::widestr& ) const -> SubQuery;
    auto  GetTermIdf( const mtc::widestr& ) const -> double;
    auto  GetTermIdf( const context::Key& ) const -> double;
  };

  auto  MiniBuilder::BuildQuery( const mtc::zval& query ) const -> SubQuery
  {
    auto  op = GetOperator( query );

    if ( op == "word" )
      return CreateWord( op.GetString() );
    if ( op == "wildcard" )
      return AsWildcard( op.GetString() );
    if ( op == "&&" || op == "quote" || op == "order" )
      return CreateArgs<true> ( mtc::api( new MiniQueryAll() ), op.GetVector() );
    if ( op == "||" )
      return CreateArgs<false>( mtc::api( new MiniQueryAny() ), op.GetVector() );
    if ( op == "fuzzy" )
    {
      auto  subset = std::vector<SubQuery>{};
      auto  quorum = 0.0;
      auto  addone = SubQuery{};
      auto  pquery = mtc::api<MiniQueryFuzzy>();

    // create subquery list
      for ( auto& next: op.GetVector() )
        if ( (addone = BuildQuery( next )).query != nullptr )
        {
          subset.push_back( addone );
          quorum += addone.range;
        }

    // create and fill the query
      pquery = new MiniQueryFuzzy( 0.7 * quorum );

      for ( auto& next: subset )
        pquery->AddQueryNode( next.query, next.range );

      return { pquery.ptr(), 0.7 * quorum };
    }
    if ( op == "cover" || op == "match" )
    {
      auto  pquery = op.GetStruct().get( "query" );
      auto  squery = SubQuery();

      if ( pquery != nullptr )  squery = BuildQuery( *pquery );
        else throw std::invalid_argument( "missing 'query' as limited object" );

      return squery;
    }

    throw std::logic_error( "operator not supported" );
  }

  template <bool Forced, class Output>
  auto  MiniBuilder::CreateArgs( mtc::api<Output> to, const mtc::array_zval& args ) const -> SubQuery
  {
    std::vector<SubQuery> queries;
    SubQuery              created;
    double                fWeight;

    if ( Forced )
    {
      fWeight = 0.0;

      for ( auto& next: args )
        if ( (created = BuildQuery( next )).query != nullptr )
        {
          fWeight = std::max( fWeight, created.range );
          queries.push_back( std::move( created ) );
        } else return { nullptr, 0.0 };
    }
      else
    {
      fWeight = 1.0;

      for ( auto& next: args )
        if ( (created = BuildQuery( next )).query != nullptr )
        {
          fWeight = std::min( fWeight, created.range );
          queries.push_back( std::move( created ) );
        }
    }

    if ( queries.empty() )
      return { nullptr, 0.0 };

    if ( queries.size() == 1 )
      return queries.front();

    for ( auto& next: queries )
      to->AddQueryNode( next.query, next.range );

    return { to.ptr(), fWeight };
  }

  auto  MiniBuilder::CreateWord( const mtc::widestr& str ) const -> SubQuery
  {
    auto  lexemes = lproc.Lemmatize( str );
    auto  ablocks = std::vector<std::pair<mtc::api<IEntities>, double>>();
    auto  fWeight = GetTermIdf( str );
    auto  pkblock = mtc::api<IEntities>();

  // check for statistics is present
    if ( fWeight <= 0.0 )
      return { nullptr, 0.0 };

  // request terms for the word
    for ( auto& lexeme: lexemes )
      if ( (pkblock = index->GetKeyBlock( { lexeme.data(), lexeme.size() } )) != nullptr )
        ablocks.emplace_back( pkblock, GetTermIdf( lexeme ) );

  // if nothing found, return nullptr
    if ( ablocks.size() == 0 )
      return { nullptr, 0.0 };

    if ( ablocks.size() == 1 )
      return { new MiniQueryTerm( ablocks.front().first, fWeight ), fWeight };

    return { new MiniMultiTerm( ablocks ), fWeight };
  }

  auto  MiniBuilder::AsWildcard( const mtc::widestr& str ) const -> SubQuery
  {
    auto  lexemes = lproc.Lemmatize( str );
    auto  ablocks = std::vector<std::pair<mtc::api<IEntities>, double>>();
    auto  fWeight = GetTermIdf( widechar('{') + str + widechar('}') );
    auto  pkblock = mtc::api<IEntities>();
    auto  wildKey = context::Key( 0xff, str );
    auto  keyList = index->ListContents( { wildKey.data(), wildKey.size() } );

    // check for statistics is present
    if ( fWeight <= 0.0 )
      return { nullptr, 0.0 };

    // enrich lexemes with index terms
    for ( auto next = keyList->Curr(); next.size() != 0; next = keyList->Next() )
      lexemes.emplace_back( next ).GetForms().set( 0xff );

    // request terms for the word
    for ( auto& next: lexemes )
      if ( (pkblock = index->GetKeyBlock( { next.data(), next.size() } )) != nullptr )
        ablocks.emplace_back( pkblock, GetTermIdf( next ) );

    // if nothing found, return nullptr
    if ( ablocks.size() == 0 )
      return { nullptr, 0.0 };

    if ( ablocks.size() == 1 )
      return { new MiniQueryTerm( ablocks.front().first, fWeight ), fWeight };

    return { new MiniMultiTerm( ablocks ), fWeight };
  }

  auto  MiniBuilder::GetTermIdf( const mtc::widestr& str ) const -> double
  {
    auto  pstats = zstat.get_zmap( str );
    auto  prange = pstats != nullptr ? pstats->get_double( "range" ) : nullptr;

    if ( prange != nullptr )
      return *prange;

    if ( pstats == nullptr )
      return -1.0;

    throw std::invalid_argument( total == 0 ?
      "terms do not have 'total' index document count" : "invalid 'terms' format" );
  }

  auto  MiniBuilder::GetTermIdf( const context::Key& key ) const -> double
  {
    auto  kstats = index->GetKeyStats( { key.data(), key.size() } );

    if ( kstats.nCount == 0 || kstats.nCount > total )
      return -1.0;

    return log( (1.0 + total) / kstats.nCount ) / log(1.0 + total);
  }

  auto  BuildMiniQuery(
    const mtc::zval&                query,
    const mtc::zmap&                terms,
    const mtc::api<IContentsIndex>& index,
    const context::Processor&       lproc, const FieldHandler& ) -> mtc::api<IQuery>
  {
    auto  zterms( terms );

    if ( zterms.empty() )
      zterms = RankQueryTerms( LoadQueryTerms( query ), index, lproc );

    return MiniBuilder( index, lproc, zterms )
      .BuildQuery( query ).query.ptr();
  }

}}
