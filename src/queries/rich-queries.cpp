# include "rich-queries.hpp"
# include "base-queries.hpp"
# include "rich-rankers.hpp"
# include "decompressor.hpp"
# include "context/processor.hpp"

namespace DelphiX {
namespace queries {

  using IEntities = IContentsIndex::IEntities;
  using Reference = IEntities::Reference;
  using RankerTag = textAPI::RankerTag;

 /*
  * RichQueryBase обеспечивает синхронное продвижение по форматам вместе с координатами
  * и передаёт распакованные форматы альтернативному методу доступа ко вхождениям.
  */
  struct RichQueryBase: IQuery
  {
    RichQueryBase( mtc::api<IEntities> fmt ):
      fmtBlock( fmt ) {}

    virtual Abstract  GetTuples( uint32_t );
    virtual Abstract  GetChunks( uint32_t, uint32_t, const Slice<const RankerTag>& ) = 0;

  protected:
    uint32_t              entityId = 0;                       // the found entity
    Abstract              abstract = {};
    mtc::api<IEntities>   fmtBlock;                           // formats and lengths
    IEntities::Reference  fmtRefer = { 0, { nullptr, 0 } };   // current formats position
  };

 /*
  * RichQueryTerm - самый простой термин с одним координатным блоком, реализующий
  * поиск и ранжирование слова с одной лексемой
  */
  struct RichQueryTerm final: RichQueryBase
  {
    implement_lifetime_control

  // construction
    RichQueryTerm( mtc::api<IEntities> ft, const mtc::api<IEntities>& bk, TermRanker&& tr ): RichQueryBase( ft ),
      entBlock( bk ),
      datatype( bk->Type() ),
      tmRanker( std::move( tr ) ) {}

  // overridables
    uint32_t  SearchDoc( uint32_t ) override;
    Abstract  GetChunks( uint32_t, uint32_t, const Slice<const RankerTag>& ) override;

  protected:
    mtc::api<IEntities> entBlock;
    const unsigned      datatype;
    TermRanker          tmRanker;
    Reference           docRefer;
    EntrySet            entryBuf[0x10000];
    EntryPos            pointBuf[0x10000];

  };

 /*
  * RichMultiTerm - коллекция их нескольких омонимов терминов, реализованная для
  * некоторой оптимизации вычислений - чтобы меньше было виртуальных вызовов и
  * больше использовался процессорный кэш
  */
  class RichMultiTerm final: public RichQueryBase
  {
    struct KeyBlock
    {
      mtc::api<IEntities> entBlock;           // coordinates
      const unsigned      datatype;
      TermRanker          tmRanker;
      Reference           docRefer = { 0, { nullptr, 0 } };   // entity reference set
      unsigned            nEntries = 0;       // number of entries
      EntrySet            entryBuf[0x10000];
      EntryPos            pointBuf[0x10000];

    // construction
      KeyBlock( const mtc::api<IEntities>& bk, TermRanker&& tr ):
        entBlock( bk ),
        datatype( bk->Type() ),
        tmRanker( std::move( tr ) )  {}

    // methods
      auto  Unpack( const Slice<const RankerTag>& fmt, unsigned id ) -> KeyBlock&
      {
        nEntries = datatype == 10 ?
          UnpackEntries<ZeroForm>( entryBuf, pointBuf, docRefer.details, tmRanker.GetRanker( fmt ), id ) :
          UnpackEntries<LoadForm>( entryBuf, pointBuf, docRefer.details, tmRanker.GetRanker( fmt ), id );
        return *this;
      }
    };

    implement_lifetime_control

  // construction
    RichMultiTerm( mtc::api<IEntities>, std::vector<std::pair<mtc::api<IEntities>, TermRanker>>& );

  // IQuery overridables
    uint32_t  SearchDoc( uint32_t ) override;
    Abstract  GetChunks( uint32_t, uint32_t, const Slice<const RankerTag>& ) override;

  protected:
    std::vector<KeyBlock>           blockSet;
    std::vector<Abstract::Entries>  selected;
    EntrySet                        entryBuf[0x10000];

  };

  class RichQueryArgs: public RichQueryBase
  {
  public:
    RichQueryArgs( mtc::api<IEntities> fmt ):
      RichQueryBase( fmt ) {}

    void   AddQueryNode( mtc::api<RichQueryBase>, double );
    auto   StrictSearch( uint32_t ) -> uint32_t;

  protected:
    struct SubQuery
    {
      mtc::api<RichQueryBase> subQuery;
      unsigned                keyOrder;
      double                  keyRange;
      double                  leastSum = 0.0;
      unsigned                docFound = 0;
      Abstract                abstract = {};


    public:
      auto  SearchDoc( uint32_t id ) -> uint32_t
      {
        if ( docFound >= id )
          return docFound;
        return abstract = {}, docFound = subQuery->SearchDoc( id );
      }
      auto  GetChunks( uint32_t id, uint32_t nw, const Slice<const RankerTag>& ft ) -> const Abstract&
      {
        return abstract.dwMode == abstract.None && docFound == id ?
          abstract = subQuery->GetChunks( id, nw, ft ) : abstract;
      }
    };

  protected:
    std::vector<SubQuery> querySet;
    EntrySet              entryBuf[0x10000];
    EntryPos              pointBuf[0x10000];

  };

  class RichQueryAll final: public RichQueryArgs
  {
    using RichQueryArgs::RichQueryArgs;

    implement_lifetime_control

    // overridables
    uint32_t  SearchDoc( uint32_t id ) override  {  return StrictSearch( id );  }
    Abstract  GetChunks( uint32_t id, uint32_t, const Slice<const RankerTag>& ) override;

  };

  class RichQueryQuote final: public RichQueryArgs
  {
    using RichQueryArgs::RichQueryArgs;

    implement_lifetime_control

    // overridables
    uint32_t  SearchDoc( uint32_t id ) override {  return StrictSearch( id );  }
    Abstract  GetChunks( uint32_t id, uint32_t, const Slice<const RankerTag>& ) override;

  };

  class RichQueryAny final: public RichQueryArgs
  {
    using RichQueryArgs::RichQueryArgs;

    implement_lifetime_control

    // overridables
    uint32_t  SearchDoc( uint32_t ) override;
    Abstract  GetChunks( uint32_t, uint32_t, const Slice<const RankerTag>& ) override;

  protected:
    std::vector<Abstract*>  selected;
  };

  class RichQuorum: public GetByQuorum
  {
    Abstract  GetTuples( uint32_t ) override  {  return {};  }
  };

  // RichQueryBase implementation

  auto  RichQueryBase::GetTuples( uint32_t tofind ) -> Abstract
  {
  // reposition the formats block to document ranked
    if ( tofind == entityId && (fmtRefer = fmtBlock->Find( tofind )).uEntity == tofind )
    {
      RankerTag format[0x1000];
      uint32_t  nWords;
      size_t    uCount = context::formats::Unpack( format,
        ::FetchFrom( fmtRefer.details.data(), nWords ), fmtRefer.details.size() );

      return GetChunks( tofind, nWords, { format, uCount } );
    }
    return {};
  }

  // RichQueryTerm implementation

 /*
  * Search next document in the list of entities
  */
  uint32_t  RichQueryTerm::SearchDoc( uint32_t tofind )
  {
    if ( (tofind = std::max( entityId, tofind )) == uint32_t(-1) )
      return entityId = uint32_t(-1);

    if ( entityId >= tofind )
      return entityId;

    return abstract = {}, entityId = (docRefer = entBlock->Find( tofind )).uEntity;
  }

 /*
  * For changed document id, unpack && return the entries and entry sets for given
  * format set
  */
  Abstract  RichQueryTerm::GetChunks( uint32_t tofind, uint32_t nWords, const Slice<const RankerTag>& fmt )
  {
    if ( docRefer.uEntity == tofind && abstract.dwMode == Abstract::None )
    {
      auto  numEnt = datatype == 20 ?
        UnpackEntries<ZeroForm>( entryBuf, pointBuf, docRefer.details, tmRanker.GetRanker( fmt ), 0 ) :
        UnpackEntries<LoadForm>( entryBuf, pointBuf, docRefer.details, tmRanker.GetRanker( fmt ), 0 );

      if ( numEnt != 0 )
        abstract = { Abstract::Rich, nWords, entryBuf, entryBuf + numEnt };
    }
    return abstract;
  }

  // RichMultiTerm implementation

  RichMultiTerm::RichMultiTerm( mtc::api<IEntities> fmt, std::vector<std::pair<mtc::api<IEntities>, TermRanker>>& terms ):
    RichQueryBase( fmt )
  {
    for ( auto& create: terms )
      blockSet.emplace_back( create.first, std::move( create.second ) );
  }

  uint32_t  RichMultiTerm::SearchDoc( uint32_t tofind )
  {
    uint32_t  uFound = uint32_t(-1);;

    if ( (tofind = std::max( tofind, entityId )) == uint32_t(-1) )
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

  Abstract  RichMultiTerm::GetChunks( uint32_t getdoc, uint32_t nwords, const Slice<const RankerTag>& fmt )
  {
    if ( abstract.entries.empty() )
    {
      size_t    nfound = 0;
      auto      outptr = entryBuf;

    // request elements in found blocks
      for ( size_t i = 0; i != blockSet.size(); ++i )
        if ( blockSet[i].docRefer.uEntity == getdoc && blockSet[i].Unpack( fmt, 0 ).nEntries != 0 )
          selected[nfound++] = { blockSet[i].entryBuf, blockSet[i].entryBuf + blockSet[i].nEntries };

    // check if single or multiple blocks
      if ( nfound == 0 )
        return abstract = {};

      if ( nfound == 1 )
        return abstract = { Abstract::Rich, nwords, selected.front() };

    // merge found tuples
      for ( auto outend = outptr + std::size(entryBuf); outptr != outend; ++outptr )
      {
        auto  select = (Abstract::Entries*)nullptr;

        for ( size_t i = 0; i != nfound; ++i )
          if ( selected[i].size() != 0 )
          {
            if ( select == nullptr
              || select->beg->limits.uMin < selected[i].beg->limits.uMin
              || (select->beg->limits.uMin == selected[i].beg->limits.uMin && select->beg->weight > selected[i].beg->weight) )
            select = &selected[i];
          }

        if ( select != nullptr )  *outptr = *select->beg++;
          else break;

        for ( size_t i = 0; i != nfound; ++i )
          if ( selected[i].beg->limits.uMin == outptr->limits.uMin )
            ++selected[i].beg;
      }
      return abstract = { Abstract::Rich, nwords, { entryBuf, outptr } };
    }
    return getdoc == entityId ? abstract : abstract = {};
  }

  // RichQueryArgs implementation

  void  RichQueryArgs::AddQueryNode( mtc::api<RichQueryBase> query, double range )
  {
    double  rgsumm = 0.0;

    querySet.push_back( { query, unsigned(querySet.size()), range } );

    std::sort( querySet.begin(), querySet.end(), []( const SubQuery& s1, const SubQuery& s2 )
      {  return s1.keyRange > s2.keyRange; } );

    for ( auto& next : querySet )
      rgsumm += next.keyRange;

    for ( auto& next : querySet )
      next.leastSum = (rgsumm -= next.keyRange);
  }

  uint32_t  RichQueryArgs::StrictSearch( uint32_t tofind )
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

  // RichQueryAll implementation

 /*
  * RichQueryAll::GetChunks( ... )
  *
  * Creates best ranked corteges of entries for '&' operator with no additional
  * checks for context except the checks provided by nested elements
  */
  Abstract  RichQueryAll::GetChunks( uint32_t udocid, uint32_t nwords, const Slice<const RankerTag>& format )
  {
    if ( abstract.dwMode != abstract.Rich )
    {
      auto  outEnt = std::begin(entryBuf);
      auto  outPos = std::begin(pointBuf);

    // request all the queries in '&' operator; not found queries force to return {}
      for ( auto& next: querySet )
        if ( next.GetChunks( udocid, nwords, format ).entries.empty() )
          return {};

    // list elements and select the best tuples for each possible compact entry;
    // shrink overlapping entries to suppress far and low-weight entries
      for ( bool hasAny = true; hasAny && outEnt != std::end(entryBuf) && outPos != std::end(pointBuf); )
      {
        auto  limits = Abstract::EntrySet::Limits{ unsigned(-1), 0 };
        auto  weight = 1.0;
        auto  center = 0.0;
        auto  sumpos = 0.0;
        auto  outOrg = outPos;

      // find element to be the lowest in the list of entries
        for ( auto& next: querySet )
        {
          auto  curpos = (next.abstract.entries.beg->limits.uMax +
            next.abstract.entries.beg->limits.uMin) / 2.0;

          weight *= (1.0 -
            next.abstract.entries.beg->weight);
          center += curpos *
            next.abstract.entries.beg->center;
          sumpos += curpos;
          limits.uMin = std::min( limits.uMin,
            next.abstract.entries.beg->limits.uMin );
          limits.uMax = std::max( limits.uMax,
            next.abstract.entries.beg->limits.uMax );
        }

      // finish weight calc
        weight = 1.0 - weight;
        center = center / sumpos;

      // check if new or intersects with previously created element
        if ( outEnt != std::begin(entryBuf) && outEnt[-1].limits.uMax >= limits.uMin && outEnt[-1].weight < weight )
          outOrg = outPos = (EntryPos*)(--outEnt)->spread.pbeg;

        for ( auto& next: querySet )
        {
        // copy relevant entries until possible overflow
          for ( auto ppos = next.abstract.entries.beg->spread.pbeg;
                     ppos < next.abstract.entries.beg->spread.pend; )
          {
            if ( outPos == std::end(pointBuf) )
              return abstract = { Abstract::Rich, nwords, { entryBuf, outEnt } };
            *outPos++ = *ppos++;
          }

        // skip lower elements
          if ( next.abstract.entries.beg->limits.uMin == limits.uMin )
            hasAny &= ++next.abstract.entries.beg != next.abstract.entries.end;
        }

        *outEnt++ = { limits, weight, center, { outOrg, outPos } };
      }
      abstract = { Abstract::Rich, nwords, { entryBuf, outEnt } };
    }
    return abstract;
  }

  // RichQueryQuote implementation

  Abstract  RichQueryQuote::GetChunks( uint32_t udocid, uint32_t nwords, const Slice<const RankerTag>& format )
  {
    if ( abstract.dwMode != abstract.Rich )
    {
      auto  outEnt = std::begin(entryBuf);
      auto  outPos = std::begin(pointBuf);

    // request all the queries in '&' operator; not found queries force to return {}
      for ( auto& next: querySet )
        if ( next.GetChunks( udocid, nwords, format ).entries.empty() )
          return {};

    // list elements and select the best tuples for each possible compact entry;
    // shrink overlapping entries to suppress far and low-weight entries
      while ( outEnt != std::end(entryBuf) )
      {
        auto  nindex = size_t(0);
        auto  begpos = unsigned{};
        auto  weight = double{};
        auto  center = double{};
        auto  ctsumm = double{};
        auto  uLower = unsigned(-1);

        // search exact sequence of elements
        while ( nindex != querySet.size() )
        {
          auto& next = querySet[nindex];
          auto& rent = next.abstract.entries;
          auto  cpos = unsigned{};

        // search first entry in next list at desired position or upper
          while ( rent.beg < rent.end && (cpos = rent.beg->limits.uMin) < begpos )
            ++rent.beg;

        // check if found
          if ( rent.empty() )
          {
            if ( outEnt != entryBuf )
              return { Abstract::Rich, nwords, { entryBuf, outEnt } };
            return {};
          }

        // get next position
          if ( nindex == 0 )
          {
            weight = 1.0 - (ctsumm = rent.beg->weight);
            center = (uLower = cpos) * rent.beg->weight;
            begpos = cpos - next.keyOrder;
            nindex = 1;
          }
            else
          if ( cpos == begpos + next.keyOrder)
          {
            weight *= 1.0 - rent.beg->weight;
            center += cpos * rent.beg->weight;
            ctsumm += rent.beg->weight;
            ++nindex;
          }
            else
          {
            begpos = cpos - next.keyOrder;
            nindex = 0;
          }
          uLower = std::min( uLower, begpos );
        }

        // register key entries
        for ( auto& next: querySet )
        {
          *outPos++ = *next.abstract.entries.beg->spread.pbeg;
            ++next.abstract.entries.beg;
          if ( outPos == std::end(pointBuf) )
            return { Abstract::Rich, nwords, { entryBuf, outEnt } };
        }

        *outEnt++ = { { uLower, unsigned(uLower + querySet.size() - 1) }, 1.0 - weight, center / ctsumm,
          { outPos - querySet.size(), outPos } };
      }
      abstract = { Abstract::Rich, nwords, { entryBuf, outEnt } };
    }
    return abstract;
  }

  // RichQueryAny implementation

  uint32_t  RichQueryAny::SearchDoc( uint32_t tofind )
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

  Abstract  RichQueryAny::GetChunks( uint32_t udocid, uint32_t nwords, const Slice<const RankerTag>& format )
  {
    if ( abstract.dwMode != abstract.Rich )
    {
      auto  outEnt = std::begin(entryBuf);
      auto  nFound = size_t(0);

    // ensure selected allocated
      if ( selected.size() != querySet.size() )
        selected.resize( querySet.size() );

    // request all the queries in '&' operator; not found queries force to return {}
      for ( auto& next: querySet )
        if ( next.GetChunks( udocid, nwords, format ).entries.size() != 0 )
          selected[nFound++] = &next.abstract;

    // list elements and select the best tuples for each possible compact entry;
    // shrink overlapping entries to suppress far and low-weight entries
      while ( outEnt != std::end(entryBuf) )
      {
        auto      plower = (Abstract*)nullptr;
        unsigned  ulower;

      // find element to be the lowest in the list of entries
        for ( size_t i = 0; i != nFound; ++i )
          if ( selected[i]->entries.size() != 0 )
          {
            auto& next = selected[i]->entries;

          // select lower element
            if ( plower != nullptr )
            {
              int rcmp = (next.beg->limits.uMin > plower->entries.beg->limits.uMin)
                       - (next.beg->limits.uMin < plower->entries.beg->limits.uMin);

              if ( rcmp < 0 || (rcmp == 0 && next.beg->weight > plower->entries.beg->weight) )
                plower = selected[i];
            } else plower = selected[i];
          }

      // check if found
        if ( plower == nullptr )
          return abstract = { Abstract::Rich, nwords, { entryBuf, outEnt } };

      // create output entry
        ulower = (*outEnt++ = *plower->entries.beg++).limits.uMin;

      // skip equal entries
        for ( size_t i = 0; i != nFound; ++i )
          if ( selected[i]->entries.size() != 0 && selected[i]->entries.beg->limits.uMin == ulower )
            ++selected[i]->entries.beg;
      }
      abstract = { Abstract::Rich, nwords, { entryBuf, outEnt } };
    }
    return abstract;
  }

  // Query creation entry

  class Builder
  {
    const mtc::zmap&                terms;
    const mtc::zmap                 zstat;
    const uint32_t                  total;
    const mtc::api<IContentsIndex>& index;
    const context::Processor&       lproc;
    const FieldHandler&             fdset;
    mtc::api<IEntities>             mkups;

  protected:
    struct Operator: std::pair<std::string, const mtc::zval&>
    {
      using std::pair<std::string, const mtc::zval&>::pair;

      bool  operator == ( const char* key ) const
        {  return first == key;  }
      auto  Arguments() const -> const mtc::array_zval&;
      auto  GetString() const -> const mtc::widestr;
    };
    struct SubQuery
    {
      mtc::api<RichQueryBase> query;
      double                  range;
    };

  public:
    Builder( const mtc::zmap& tm, const mtc::api<IContentsIndex>& dx, const context::Processor& lp, const FieldHandler& fs ):
      terms( tm ),
      zstat( tm.get_zmap( "stats", {} ) ),
      total( terms.get_word32( "total", 0 ) ),
      index( dx ),
      lproc( lp ),
      fdset( fs ),
      mkups( index->GetKeyBlock( "fmt" ) )  {}

    auto  BuildQuery( const mtc::zval& ) const -> SubQuery;

    auto  GetCommand( const mtc::zval& ) const -> Operator;
  template <bool Forced, class Output>
    auto  CreateArgs( mtc::api<Output>, const mtc::array_zval& ) const -> SubQuery;
    auto  CreateWord( const mtc::widestr& ) const -> SubQuery;
    auto  MakeRanker( double flbase, const context::Lexeme& ) const -> TermRanker;
    auto  GetTermIdf( const mtc::widestr& ) const -> double;
  };

  auto  Builder::BuildQuery( const mtc::zval& query ) const -> SubQuery
  {
    auto  op = GetCommand( query );

    if ( op == "&&" )
      return CreateArgs<true> ( mtc::api( new RichQueryAll( mkups ) ), op.Arguments() );
    if ( op == "||" )
      return CreateArgs<false>( mtc::api( new RichQueryAny( mkups ) ), op.Arguments() );
    if ( op == "quote" || op == "order" )
      return CreateArgs<true> ( mtc::api( new RichQueryQuote( mkups ) ), op.Arguments() );
    if ( op == "word" )
      return CreateWord( op.GetString() );

    throw std::logic_error( "operator not supported" );
  }

  auto  Builder::GetCommand( const mtc::zval& query ) const -> Operator
  {
    switch ( query.get_type() )
    {
      case mtc::zval::z_charstr:
      case mtc::zval::z_widestr:
        return { "word", query };
      case mtc::zval::z_zmap:
      {
        auto& zmap = *query.get_zmap();
        auto  pbeg = zmap.begin();
        auto  xend = zmap.begin();

        if ( pbeg == zmap.end() )
          throw std::invalid_argument( "invalid (empty) query passed" );
        if ( ++xend != zmap.end() )
          throw std::invalid_argument( "query has more than one operator entries" );
        if ( !pbeg->first.is_charstr() )
          throw std::invalid_argument( "query operator key has to be string" );
        return { pbeg->first.to_charstr(), pbeg->second };
      }
      default:
        throw std::invalid_argument( "invalid query type" );
    }
  }

  template <bool Forced, class Output>
  auto  Builder::CreateArgs( mtc::api<Output> to, const mtc::array_zval& args ) const -> SubQuery
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

  auto  Builder::CreateWord( const mtc::widestr& str ) const -> SubQuery
  {
    auto  lexemes = lproc.Lemmatize( str );
    auto  ablocks = std::vector<std::pair<mtc::api<IEntities>, TermRanker>>();
    auto  fWeight = GetTermIdf( str );
    auto  pkblock = mtc::api<IEntities>();

  // check for statistics is present
    if ( fWeight <= 0.0 )
      return { nullptr, 0.0 };

  // request terms for the word
    for ( auto& lexeme: lexemes )
      if ( (pkblock = index->GetKeyBlock( lexeme )) != nullptr )
        ablocks.emplace_back( pkblock, MakeRanker( fWeight, lexeme ) );

  // if nothing found, return nullptr
    if ( ablocks.size() == 0 )
      return { nullptr, 0.0 };

    if ( ablocks.size() != 1 )
      return { new RichMultiTerm( mkups, ablocks ), fWeight };

    return { new RichQueryTerm( mkups, ablocks.front().first, std::move( ablocks.front().second ) ), fWeight };
  }

  auto  Builder::MakeRanker( double flbase, const context::Lexeme& lexeme ) const -> TermRanker
  {
    TermRanker  ranker;

    return ranker;
  }

  auto  Builder::GetTermIdf( const mtc::widestr& str ) const -> double
  {
    auto  pstats = zstat.get_zmap( str );
    auto  prange = pstats != nullptr ? pstats->get_double( "range" ) : nullptr;

    if ( prange != nullptr )  return *prange;
      else
    if ( pstats == nullptr )  return -1.0;
      else
    {
      if ( total == 0 )
        throw std::invalid_argument( "terms do not have 'total' index document count" );
      throw std::invalid_argument( "invalid 'terms' format" );
    }
  }

  // Builder::Operator implementation

  auto  Builder::Operator::Arguments() const -> const mtc::array_zval&
  {
    if ( second.get_type() != mtc::zval::z_array_zval )
      throw std::invalid_argument( "operator value has to point to array" );
    return *second.get_array_zval();
  }

  auto  Builder::Operator::GetString() const -> const mtc::widestr
  {
    if ( second.get_type() == mtc::zval::z_charstr )
      return codepages::mbcstowide( codepages::codepage_utf8, *second.get_charstr() );
    if ( second.get_type() == mtc::zval::z_widestr )
      return *second.get_widestr();
    throw std::invalid_argument( "Operator has to handle string" );
  }

  auto  GetRichQuery(
    const mtc::zmap&                query,
    const mtc::zmap&                terms,
    const mtc::api<IContentsIndex>& index,
    const context::Processor&       lproc,
    const FieldHandler&             fdset ) -> mtc::api<IQuery>
  {
    return Builder( terms, index, lproc, fdset ).BuildQuery( query ).query.ptr();
  }

}}
