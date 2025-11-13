# include "../../queries/builder.hpp"
# include "../../compat.hpp"
# include "rich-rankers.hpp"
# include "query-tools.hpp"
# include "field-set.hpp"
# include "decompressor.hpp"
# include "context/processor.hpp"
# include <mtc/bitset.h>

namespace DelphiX {
namespace queries {

  using EntryPos = Abstract::EntryPos;
  using EntrySet = Abstract::EntrySet;

  using IEntities = IContentsIndex::IEntities;
  using Reference = IEntities::Reference;
  using RankerTag = context::RankerTag;

 /*
  * RichQueryBase обеспечивает синхронное продвижение по форматам вместе с координатами
  * и передаёт распакованные форматы альтернативному методу доступа ко вхождениям.
  */
  struct RichQueryBase: IQuery
  {
    RichQueryBase( mtc::api<IEntities> fmt ):
      fmtBlock( fmt ) {}

    virtual Abstract  GetTuples( uint32_t );
    virtual Abstract& GetChunks( uint32_t, const mtc::span<const RankerTag>& ) = 0;

  protected:
    auto  SetPoints( EntryPos*, const EntryPos*, const EntrySet& ) const -> EntryPos*;

    mtc::api<IEntities>   fmtBlock;                           // formats and lengths
    IEntities::Reference  fmtRefer = { 0, { nullptr, 0 } };   // current formats position
    uint32_t              entityId = 0;                       // the found entity
    Abstract              abstract = {};
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
    Abstract& GetChunks( uint32_t, const mtc::span<const RankerTag>& ) override;

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

    // construction
      KeyBlock( const mtc::api<IEntities>& bk, TermRanker&& tr ):
        entBlock( bk ),
        datatype( bk->Type() ),
        tmRanker( std::move( tr ) )  {}

    // methods
      auto  Unpack( EntrySet* tuples, EntryPos* points, size_t maxlen,
        const mtc::span<const RankerTag>& format, unsigned id ) -> unsigned
      {
        return
          datatype == 20 ? UnpackEntries<ZeroForm>( tuples, points, maxlen, docRefer.details, tmRanker.GetRanker( format ), id ) :
          datatype == 21 ? UnpackEntries<LoadForm>( tuples, points, maxlen, docRefer.details, tmRanker.GetRanker( format ), id ) :
          throw std::logic_error( "unknown block type @" __FILE__ ":" LINE_STRING );
      }
    };

    implement_lifetime_control

  // construction
    RichMultiTerm( mtc::api<IEntities>, std::vector<std::pair<mtc::api<IEntities>, TermRanker>>& );

  // IQuery overridables
    uint32_t  SearchDoc( uint32_t ) override;
    Abstract& GetChunks( uint32_t, const mtc::span<const RankerTag>& ) override;

  protected:
    std::vector<KeyBlock>           blockSet;
    std::vector<Abstract::Entries>  selected;
    std::vector<EntrySet>           entryBuf;
    std::vector<EntrySet>           entryOut;
    std::vector<EntryPos>           pointBuf;

  };

  class RichQueryArgs: public RichQueryBase
  {
  public:
    RichQueryArgs( mtc::api<IEntities> fmt ):
      RichQueryBase( fmt ),
      entryBuf( 0x10000 ),
      pointBuf( 0x10000 ),
      entryEnd( entryBuf.data() + entryBuf.size() ),
      pointEnd( pointBuf.data() + pointBuf.size() ) {}

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

    // methods
      auto  SearchDoc( uint32_t id ) -> uint32_t
      {
        if ( docFound >= id )
          return docFound;
        return abstract = {}, docFound = subQuery->SearchDoc( id );
      }
      auto  GetChunks( uint32_t id, const mtc::span<const RankerTag>& ft ) -> const Abstract&
      {
        return abstract.dwMode == abstract.None && docFound == id ?
          abstract = subQuery->GetChunks( id, ft ) : abstract;
      }
    };

  protected:
    std::vector<SubQuery> querySet;
    std::vector<EntrySet> entryBuf;
    std::vector<EntryPos> pointBuf;
    const EntrySet*       entryEnd;
    const EntryPos*       pointEnd;

  };

  class RichQueryAll final: public RichQueryArgs
  {
    using RichQueryArgs::RichQueryArgs;

    implement_lifetime_control

    // overridables
    uint32_t  SearchDoc( uint32_t id ) override  {  return StrictSearch( id );  }
    Abstract& GetChunks( uint32_t id, const mtc::span<const RankerTag>& ) override;

  };

  class RichQueryOrder final: public RichQueryArgs
  {
    using RichQueryArgs::RichQueryArgs;

    implement_lifetime_control

    // overridables
    uint32_t  SearchDoc( uint32_t id ) override {  return StrictSearch( id );  }
    Abstract& GetChunks( uint32_t id, const mtc::span<const RankerTag>& ) override;

  };

  class RichQueryFuzzy final: public RichQueryArgs
  {
    using RichQueryArgs::RichQueryArgs;

    implement_lifetime_control

    // construction
    RichQueryFuzzy( mtc::api<IEntities> fmt, double quo ): RichQueryArgs( fmt ),
      quorum( quo ) {}

    // overridables
    uint32_t  SearchDoc( uint32_t id ) override;
    Abstract& GetChunks( uint32_t id, const mtc::span<const RankerTag>& ) override;

    double    DistRange( int distance ) const
    {
      return distance >= 0 ?
        0.05 + 0.95 * pow(cos(atan(fabs(distance / 5.0))), 3) :
        0.05 + 0.85 * pow(cos(atan(fabs(distance / 5.0))), 3);
    }
  protected:
    double  quorum;

  };

  class RichQueryAny final: public RichQueryArgs
  {
    using RichQueryArgs::RichQueryArgs;

    implement_lifetime_control

    // overridables
    uint32_t  SearchDoc( uint32_t ) override;
    Abstract& GetChunks( uint32_t, const mtc::span<const RankerTag>& ) override;

  protected:
    std::vector<Abstract*>  selected;
  };

  class RichQueryField: public RichQueryBase
  {
  public:
  // construction
    RichQueryField( mtc::api<IEntities>, const std::vector<unsigned>&, mtc::api<RichQueryBase> );

    // overridables
    uint32_t  SearchDoc( uint32_t id ) override;

  protected:
    mtc::api<RichQueryBase> subQuery;
    std::vector<unsigned>   matchSet;
    EntrySet                entryBuf[0x10000];

  };

  class RichQueryCover final: public RichQueryField
  {
    using RichQueryField::RichQueryField;

    implement_lifetime_control

  // overridables
    Abstract& GetChunks( uint32_t id, const mtc::span<const RankerTag>& ) override;
  };

  class RichQueryMatch final: public RichQueryField
  {
    using RichQueryField::RichQueryField;

    implement_lifetime_control

  // overridables
    Abstract& GetChunks( uint32_t id, const mtc::span<const RankerTag>& ) override;
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
      auto&     report = GetChunks( tofind, { format, uCount } );

      return report.nWords = nWords, report;
    }
    return abstract = {};
  }

  inline
  auto  RichQueryBase::SetPoints( EntryPos* out, const EntryPos* lim, const EntrySet& ent ) const -> EntryPos*
  {
    for ( auto beg = ent.spread.pbeg; out != lim && beg != ent.spread.pend; )
      *out++ = *beg++;
    return out;
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
  Abstract& RichQueryTerm::GetChunks( uint32_t tofind, const mtc::span<const RankerTag>& fmt )
  {
    if ( docRefer.uEntity == tofind && abstract.dwMode == Abstract::None )
    {
      auto  numEnt = datatype == 20 ?
        UnpackEntries<ZeroForm>( entryBuf, pointBuf, docRefer.details, tmRanker.GetRanker( fmt ), 0 ) :
        UnpackEntries<LoadForm>( entryBuf, pointBuf, docRefer.details, tmRanker.GetRanker( fmt ), 0 );

      if ( numEnt != 0 )
        abstract = { Abstract::Rich, 0, entryBuf, entryBuf + numEnt };
    }
    return abstract;
  }

  // RichMultiTerm implementation

  RichMultiTerm::RichMultiTerm( mtc::api<IEntities> fmt, std::vector<std::pair<mtc::api<IEntities>, TermRanker>>& terms ):
    RichQueryBase( fmt ),
    entryBuf( 0x10000 ),
    entryOut( 0x10000 ),
    pointBuf( 0x10000 )
  {
    for ( auto& create: terms )
      blockSet.emplace_back( create.first, std::move( create.second ) );

    selected.resize( blockSet.size() );
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

  Abstract& RichMultiTerm::GetChunks( uint32_t getdoc, const mtc::span<const RankerTag>& fmt )
  {
    if ( abstract.entries.empty() )
    {
      size_t  nfound = 0;
      size_t  nstart = 0;
      auto    entPtr = entryBuf.data();
      auto    posPtr = pointBuf.data();
      auto    outPtr = entryOut.data();

    // request elements in found blocks
      for ( size_t i = 0; i != blockSet.size(); ++i )
        if ( blockSet[i].docRefer.uEntity == getdoc )
        {
          auto  ucount = blockSet[i].Unpack( entPtr, posPtr, entryBuf.data() + entryBuf.size() - entPtr, fmt, 0 );

          if ( ucount != 0 )
          {
            selected[nfound++] = { entPtr, entPtr + ucount };
              entPtr += ucount;
              posPtr += ucount;
          }
        }

    // check if single or multiple blocks
      if ( nfound == 0 )
        return abstract = {};

      if ( nfound == 1 )
        return abstract = { Abstract::Rich, 0, selected.front() };

    // merge found tuples
      for ( auto outend = entryOut.data() + entryOut.size(); outPtr != outend && nstart < nfound; ++outPtr )
        if ( selected[nstart].pbeg < selected[nstart].pend )
        {
          auto  uLower = (*outPtr = *selected[nstart].pbeg).limits.uMin;

          if ( ++selected[nstart].pbeg == selected[nstart].pend )
            ++nstart;

          for ( auto norder = nstart + 1; norder < nfound; ++norder )
            if ( uLower == selected[norder].pbeg->limits.uMin )
              ++selected[norder].pbeg;
        }
      return abstract = { Abstract::Rich, 0, { entryOut.data(), outPtr } };
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
  Abstract& RichQueryAll::GetChunks( uint32_t udocid, const mtc::span<const RankerTag>& format )
  {
    if ( abstract.dwMode != abstract.Rich )
    {
      auto  outEnt = entryBuf.data();
      auto  outPos = pointBuf.data();

    // request all the queries in '&' operator; not found queries force to return {}
      for ( auto& next: querySet )
        if ( next.GetChunks( udocid, format ).entries.empty() )
          return abstract = {};

    // list elements and select the best tuples for each possible compact entry;
    // shrink overlapping entries to suppress far and low-weight entries
      for ( bool hasAny = true; hasAny && outEnt != entryEnd && outPos != pointEnd; )
      {
        auto  limits = Abstract::EntrySet::Limits{ unsigned(-1), 0 };
        auto  weight = 1.0;
        auto  center = 0.0;
        auto  sumpos = 0.0;
        auto  outOrg = outPos;

      // find element to be the lowest in the list of entries
        for ( auto& next: querySet )
        {
          auto  curpos = (next.abstract.entries.pbeg->limits.uMax +
            next.abstract.entries.pbeg->limits.uMin) / 2.0;

          weight *= (1.0 -
            next.abstract.entries.pbeg->weight);
          center += curpos *
            next.abstract.entries.pbeg->center;
          sumpos += curpos;
          limits.uMin = std::min( limits.uMin,
            next.abstract.entries.pbeg->limits.uMin );
          limits.uMax = std::max( limits.uMax,
            next.abstract.entries.pbeg->limits.uMax );
        }

      // finish weight calc
        weight = 1.0 - weight;
        center = center / sumpos;

      // check if new or intersects with previously created element
        if ( outEnt != entryBuf.data() && outEnt[-1].limits.uMax >= limits.uMin && outEnt[-1].weight < weight )
          outOrg = outPos = (EntryPos*)(--outEnt)->spread.pbeg;

        for ( auto& next: querySet )
        {
        // copy relevant entries until possible overflow
          if ( (outPos = SetPoints( outPos, pointEnd, *next.abstract.entries.pbeg )) == pointEnd )
            return abstract = { Abstract::Rich, 0, { entryBuf.data(), outEnt } };

        // skip lower elements
          if ( next.abstract.entries.pbeg->limits.uMin == limits.uMin )
            hasAny &= ++next.abstract.entries.pbeg != next.abstract.entries.pend;
        }

        *outEnt++ = { limits, weight, center, { outOrg, outPos } };
      }
      abstract = { Abstract::Rich, 0, { entryBuf.data(), outEnt } };
    }
    return abstract;
  }

  // RichQueryOrder implementation

  Abstract& RichQueryOrder::GetChunks( uint32_t udocid, const mtc::span<const RankerTag>& format )
  {
    if ( abstract.dwMode != abstract.Rich )
    {
      auto  outEnt = entryBuf.data();
      auto  outPos = pointBuf.data();

    // request all the queries in '&' operator; not found queries force to return {}
      for ( auto& next: querySet )
        if ( next.GetChunks( udocid, format ).entries.empty() )
          return abstract = {};

    // list elements and select the best tuples for each possible compact entry;
    // shrink overlapping entries to suppress far and low-weight entries
      while ( outEnt != entryEnd )
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
          while ( rent.pbeg < rent.pend && (cpos = rent.pbeg->limits.uMin) < begpos )
            ++rent.pbeg;

        // check if found
          if ( rent.empty() )
          {
            if ( outEnt != entryBuf.data() )
              return abstract = { Abstract::Rich, 0, { entryBuf.data(), outEnt } };
            return abstract = {};
          }

        // get next position
          if ( nindex++ == 0 )
          {
            weight = 1.0 - (ctsumm = rent.pbeg->weight);
            center = (uLower = cpos) * rent.pbeg->weight;
            begpos = cpos - next.keyOrder;
          }
            else
          if ( cpos == begpos + next.keyOrder)
          {
            weight *= 1.0 - rent.pbeg->weight;
            center += cpos * rent.pbeg->weight;
            ctsumm += rent.pbeg->weight;
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
          *outPos++ = *next.abstract.entries.pbeg->spread.pbeg;
            ++next.abstract.entries.pbeg;
          if ( outPos == pointEnd )
            return abstract = { Abstract::Rich, 0, { entryBuf.data(), outEnt } };
        }

        *outEnt++ = { { uLower, unsigned(uLower + querySet.size() - 1) }, 1.0 - weight, center / ctsumm,
          { outPos - querySet.size(), outPos } };
      }
      abstract = { Abstract::Rich, 0, { entryBuf.data(), outEnt } };
    }
    return abstract;
  }

  // RichQueryFuzzy implementation

  uint32_t  RichQueryFuzzy::SearchDoc( uint32_t tofind )
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

  Abstract& RichQueryFuzzy::GetChunks( uint32_t  udocid, const mtc::span<const RankerTag>& format )
  {
    if ( abstract.dwMode != abstract.Rich )
    {
      auto  outEnt = entryBuf.data();
      auto  outPos = pointBuf.data();

    // request all the queries in 'fuzzy operator; by the request, quorum may be accessed
    // with this query and at least one element is available and provides the quorum
      for ( auto& next: querySet )
        next.GetChunks( udocid, format );

    // list elements and select the best tuples for each possible compact entry;
    // shrink overlapping entries to suppress far and low-weight entries
      while ( outEnt != entryEnd && outPos != pointEnd )
      {
        auto  center = double(0);     // расчёт центроида веса
        auto  ctsumm = double(0);     // сумма весов орт
        auto  uLower = unsigned(-1);
        auto  uUpper = unsigned(0);
        auto  scalar = double(0.0);
        auto  en_len = double(0.0);
        auto  quo_fl = double(0.0);
        auto  weight = double{};
        auto  outOrg = outPos;
        int   despos;                 // desired position

        // search exact sequence of elements
        for ( auto& rquery: querySet )
        {
          auto& rentry = rquery.abstract.entries;

        // check if has the entries; use entries weight in possible quorum
          if ( rentry.size() != 0 )
          {
            despos = uLower == unsigned(-1) ? unsigned(rentry.pbeg->center) : unsigned(despos + rquery.keyOrder);
            uLower = std::min( uLower, rentry.pbeg->limits.uMin );
            uUpper = std::max( uUpper, rentry.pbeg->limits.uMax );
            scalar += rentry.pbeg->weight * DistRange( int(rentry.pbeg->center - despos) );
            en_len += rentry.pbeg->weight * rentry.pbeg->weight;
            center += rentry.pbeg->weight * rentry.pbeg->center;
            ctsumm += rentry.pbeg->weight;
            quo_fl += rquery.keyRange;
          }
        }

        // check if element is found
        if ( uLower != unsigned(-1) )
        {
          bool  useEnt = quo_fl >= quorum && (weight = scalar / sqrt(querySet.size() * en_len)) >= 0.01;

          if ( useEnt && outEnt != entryBuf.data() && outEnt[-1].limits.uMax >= uLower )
          {
            if ( (useEnt = (outEnt[-1].weight < weight)) == true )
              outOrg = outPos = (EntryPos*)(--outEnt)->spread.pbeg;
          }

          // skip to the next possible tuple
          for ( auto& rquery: querySet )
          {
            auto& rentry = rquery.abstract.entries;

            if ( rentry.size() != 0 )
            {
              if ( useEnt )
                outPos = SetPoints( outPos, pointEnd, *rentry.pbeg );

              if ( rquery.abstract.entries.pbeg->limits.uMin == uLower )
                ++rquery.abstract.entries.pbeg;
            }
          }

          if ( useEnt )
            *outEnt++ = { { uLower, uUpper }, weight, center / ctsumm, { outOrg, outPos } };
        } else break;
      }
      abstract = { outEnt != entryBuf.data() ? Abstract::Rich : Abstract::None, 0,
        { entryBuf.data(), outEnt } };
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

  Abstract& RichQueryAny::GetChunks( uint32_t udocid, const mtc::span<const RankerTag>& format )
  {
    if ( abstract.dwMode != abstract.Rich )
    {
      auto  outEnt = entryBuf.data();
      auto  nFound = size_t(0);

    // ensure selected allocated
      if ( selected.size() != querySet.size() )
        selected.resize( querySet.size() );

    // request all the queries in '&' operator; not found queries force to return {}
      for ( auto& next: querySet )
        if ( next.GetChunks( udocid, format ).entries.size() != 0 )
          selected[nFound++] = &next.abstract;

    // list elements and select the best tuples for each possible compact entry;
    // shrink overlapping entries to suppress far and low-weight entries
      while ( outEnt != entryEnd )
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
              int  rcmp = (next.pbeg->limits.uMin > plower->entries.pbeg->limits.uMin)
                        - (next.pbeg->limits.uMin < plower->entries.pbeg->limits.uMin);

              if ( rcmp < 0 || (rcmp == 0 && next.pbeg->weight > plower->entries.pbeg->weight) )
                plower = selected[i];
            } else plower = selected[i];
          }

      // check if found
        if ( plower == nullptr )
          return abstract = { Abstract::Rich, 0, { entryBuf.data(), outEnt } };

      // create output entry
        ulower = (*outEnt++ = *plower->entries.pbeg++).limits.uMin;

      // skip equal entries
        for ( size_t i = 0; i != nFound; ++i )
          if ( selected[i]->entries.size() != 0 && selected[i]->entries.pbeg->limits.uMin == ulower )
            ++selected[i]->entries.pbeg;
      }
      abstract = { Abstract::Rich, 0, { entryBuf.data(), outEnt } };
    }
    return abstract;
  }

// RichQueryField implementation

  RichQueryField::RichQueryField(
    mtc::api<IEntities>           fmt,
    const std::vector<unsigned>&  fds,
    mtc::api<RichQueryBase>       sub ): RichQueryBase( fmt ),
      subQuery( sub )
  {
    for ( auto id: fds )
      mtc::bitset_set( matchSet, id );
  }

  auto  RichQueryField::SearchDoc( uint32_t id ) -> uint32_t
  {
    return entityId < id ? abstract = {},
      entityId = subQuery->SearchDoc( id ) : entityId;
  }

  // RichQueryCover implementation

  Abstract& RichQueryCover::GetChunks( uint32_t id, const mtc::span<const RankerTag>& ft )
  {
    if ( id == entityId && abstract.dwMode != abstract.Rich )
    {
      auto  subEnt = subQuery->GetChunks( id, ft );
      auto  tagBeg = ft.data();
      auto  tagEnd = ft.data() + ft.size();
      auto  outPtr = entryBuf;

      for ( ; subEnt.entries.pbeg != subEnt.entries.pend && tagBeg != tagEnd; ++subEnt.entries.pbeg )
      {
        while ( tagBeg != tagEnd && tagBeg->uUpper < subEnt.entries.pbeg->limits.uMax && !mtc::bitset_get( matchSet, tagBeg->format ) )
          ++tagBeg;
        if ( tagBeg == tagEnd )
          break;
        while ( subEnt.entries.pbeg != subEnt.entries.pend && subEnt.entries.pbeg->limits.uMin < tagBeg->uLower )
          ++subEnt.entries.pbeg;
        if ( subEnt.entries.pbeg == subEnt.entries.pend )
          break;
        if ( tagBeg->uLower <= subEnt.entries.pbeg->limits.uMin
          && tagBeg->uUpper >= subEnt.entries.pbeg->limits.uMax )
            *outPtr++ = *subEnt.entries.pbeg;
      }

      if ( outPtr != entryBuf )
        abstract = { Abstract::Rich, 0, { entryBuf, outPtr } };
    }
    return abstract;
  }

  // RichQueryMatch implementation

  Abstract& RichQueryMatch::GetChunks( uint32_t id, const mtc::span<const RankerTag>& ft )
  {
    if ( id == entityId && abstract.dwMode != abstract.Rich )
    {
      auto  subEnt = subQuery->GetChunks( id, ft );
      auto  tagBeg = ft.data();
      auto  tagEnd = ft.data() + ft.size();
      auto  outPtr = entryBuf;

      for ( ; subEnt.entries.pbeg != subEnt.entries.pend && tagBeg != tagEnd; ++subEnt.entries.pbeg )
      {
        while ( tagBeg != tagEnd && tagBeg->uLower < subEnt.entries.pbeg->limits.uMin && !mtc::bitset_get( matchSet, tagBeg->format ) )
          ++tagBeg;
        if ( tagBeg == tagEnd )
          break;
        while ( subEnt.entries.pbeg != subEnt.entries.pend && subEnt.entries.pbeg->limits.uMin < tagBeg->uLower )
          ++subEnt.entries.pbeg;
        if ( subEnt.entries.pbeg == subEnt.entries.pend )
          break;
        if ( tagBeg->uLower == subEnt.entries.pbeg->limits.uMin
          && tagBeg->uUpper == subEnt.entries.pbeg->limits.uMax )
            *outPtr++ = *subEnt.entries.pbeg;
      }

      if ( outPtr != entryBuf )
        abstract = { Abstract::Rich, 0, { entryBuf, outPtr } };
    }
    return abstract;
  }

  // Query creation entry

  class RichBuilder
  {
    const mtc::zmap&                terms;
    const mtc::zmap                 zstat;
    const uint32_t                  total;
    const mtc::api<IContentsIndex>& index;
    const context::Processor&       lproc;
    const FieldHandler&             fdhan;
    mtc::api<IEntities>             mkups;

    struct QuerySettings
    {
      FieldSet  fieldSet;
      unsigned  uContext = unsigned(-1);
      bool      isStrict = false;

      auto  SetStrict( bool strict ) const -> QuerySettings
        {  return strict == isStrict ? *this : QuerySettings{ fieldSet, uContext, strict };  }
    };

  protected:
    struct SubQuery
    {
      mtc::api<RichQueryBase> query;
      double                  range;
    };

  public:
    RichBuilder( const mtc::zmap& tm, const mtc::api<IContentsIndex>& dx, const context::Processor& lp, const FieldHandler& fs ):
      terms( tm ),
      zstat( tm.get_zmap( "terms-range-map", {} ) ),
      total( terms.get_word32( "collection-size", 0 ) ),
      index( dx ),
      lproc( lp ),
      fdhan( fs ),
      mkups( index->GetKeyBlock( "fmt" ) )  {}

    auto  BuildQuery( const mtc::zval&, const QuerySettings& ) const -> SubQuery;
  template <bool Forced, class Output>
    auto  CreateArgs( mtc::api<Output>, const mtc::array_zval&, const QuerySettings& ) const -> SubQuery;
    auto  CreateWord( const mtc::widestr&, const QuerySettings& ) const -> SubQuery;
    auto  AsWildcard( const mtc::widestr&, const QuerySettings& ) const -> SubQuery;
    auto  GetTermIdf( const mtc::widestr& ) const -> double;
    auto  GetTermIdf( const context::Key& ) const -> double;
    auto  LoadFields( const mtc::zval& ) const -> FieldSet;
  };

  auto  RichBuilder::BuildQuery( const mtc::zval& query, const QuerySettings& sets ) const -> SubQuery
  {
    auto  op = GetOperator( query );

    if ( op == "word" )
      return CreateWord( op.GetString(), sets );
    if ( op == "wildcard" )
      return AsWildcard( op.GetString(), sets );
    if ( op == "&&" )
      return CreateArgs<true> ( mtc::api( new RichQueryAll( mkups ) ), op.GetVector(), sets );
    if ( op == "||" )
      return CreateArgs<false>( mtc::api( new RichQueryAny( mkups ) ), op.GetVector(), sets );
    if ( op == "quote" || op == "order" )
    {
      return CreateArgs<true> ( mtc::api( new RichQueryOrder( mkups ) ), op.GetVector(),
        sets.SetStrict( op == "quote" ) );
    }
    if ( op == "fuzzy" )
    {
      auto  subset = std::vector<SubQuery>{};
      auto  quorum = 0.0;
      auto  addone = SubQuery{};
      auto  pquery = mtc::api<RichQueryFuzzy>();

    // create subquery list
      for ( auto& next: op.GetVector() )
        if ( (addone = BuildQuery( next, sets )).query != nullptr )
        {
          subset.push_back( addone );
          quorum += addone.range;
        }

    // create and fill the query
      pquery = new RichQueryFuzzy( mkups, 0.7 * quorum );

      for ( auto& next: subset )
        pquery->AddQueryNode( next.query, next.range );

      return { pquery.ptr(), 0.7 * quorum };
    }
    if ( op == "cover" || op == "match" )
    {
      auto  pnames = op.GetStruct().get( "field" );
      auto  pquery = op.GetStruct().get( "query" );
      auto  fields = FieldSet();
      auto  squery = SubQuery();

      if ( pnames != nullptr )  fields = std::move( LoadFields( *pnames ) );
        else throw std::invalid_argument( "missing 'field' as field limitation" );

      if ( pquery != nullptr )  squery = BuildQuery( *pquery, sets );
        else throw std::invalid_argument( "missing 'query' as limited object" );

      if ( op == "match" )
        return { new RichQueryMatch( mkups, fields, squery.query ), squery.range };
      else
        return { new RichQueryCover( mkups, fields, squery.query ), squery.range };
    }
    throw std::logic_error( "operator not supported" );
  }

  template <bool Forced, class Output>
  auto  RichBuilder::CreateArgs( mtc::api<Output> to, const mtc::array_zval& args, const QuerySettings& fds ) const -> SubQuery
  {
    std::vector<SubQuery> queries;
    SubQuery              created;
    double                fWeight;

    if ( Forced )
    {
      fWeight = 0.0;

      for ( auto& next: args )
        if ( (created = BuildQuery( next, fds )).query != nullptr )
        {
          fWeight = std::max( fWeight, created.range );
          queries.push_back( std::move( created ) );
        } else return { nullptr, 0.0 };
    }
      else
    {
      fWeight = 1.0;

      for ( auto& next: args )
        if ( (created = BuildQuery( next, fds )).query != nullptr )
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

  auto  RichBuilder::CreateWord( const mtc::widestr& str, const QuerySettings& sets ) const -> SubQuery
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
      if ( (pkblock = index->GetKeyBlock( { lexeme.data(), lexeme.size() } )) != nullptr )
        ablocks.emplace_back( pkblock, TermRanker( sets.fieldSet, lexeme, fWeight, false ) );

  // if nothing found, return nullptr
    if ( ablocks.size() == 0 )
      return { nullptr, 0.0 };

    if ( ablocks.size() != 1 )
      return { new RichMultiTerm( mkups, ablocks ), fWeight };

    return { new RichQueryTerm( mkups, ablocks.front().first, std::move( ablocks.front().second ) ), fWeight };
  }

  auto  RichBuilder::AsWildcard( const mtc::widestr& str, const QuerySettings& sets ) const -> SubQuery
  {
    auto  lexemes = lproc.Lemmatize( str );
    auto  ablocks = std::vector<std::pair<mtc::api<IEntities>, TermRanker>>();
    auto  fWeight = GetTermIdf( widechar('{') + str + widechar('}') );
    auto  pkblock = mtc::api<IEntities>();
    auto  wildKey = context::Key( 0xff, codepages::strtolower( str ) );
    auto  keyList = index->ListContents( { wildKey.data(), wildKey.size() } );

    // check for statistics is present
    if ( fWeight <= 0.01 )    // check if placeholder
      return { nullptr, 0.0 };

    // create cooblocks
    for ( auto& next: lexemes )
      if ( (pkblock = index->GetKeyBlock( { next.data(), next.size() } )) != nullptr )
        ablocks.emplace_back( pkblock, TermRanker( sets.fieldSet, next, GetTermIdf( next ), true ) );

    // enrich lexemes with index terms
    for ( auto next = keyList->Curr(); next.size() != 0; next = keyList->Next() )
      if ( (pkblock = index->GetKeyBlock( next )) != nullptr )
        ablocks.emplace_back( pkblock, TermRanker( sets.fieldSet, next, GetTermIdf( next ), true ) );

    // if nothing found, return nullptr
    if ( ablocks.size() == 0 )
      return { nullptr, 0.0 };

    if ( ablocks.size() == 1 )
      return { new RichQueryTerm( mkups, ablocks.front().first, std::move( ablocks.front().second ) ), fWeight };

    return { new RichMultiTerm( mkups, ablocks ), fWeight };
  }

  auto  RichBuilder::GetTermIdf( const mtc::widestr& str ) const -> double
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

  auto  RichBuilder::GetTermIdf( const context::Key& key ) const -> double
  {
    auto  kstats = index->GetKeyStats( { key.data(), key.size() } );

    if ( kstats.nCount == 0 || kstats.nCount > total )
      return -1.0;

    return log( (1.0 + total) / kstats.nCount ) / log(1.0 + total);
  }

  auto  RichBuilder::LoadFields( const mtc::zval& fds ) const -> FieldSet
  {
    auto  fields = FieldSet();

    switch ( fds.get_type() )
    {
      case mtc::zval::z_charstr:
        return { fdhan, *fds.get_charstr() };
      case mtc::zval::z_array_charstr:
        return { fdhan, *fds.get_array_charstr() };
      default:
        throw std::invalid_argument( "invalid 'field' format" );
    }
  }

  auto  BuildRichQuery(
    const mtc::zval&                query,
    const mtc::zmap&                terms,
    const mtc::api<IContentsIndex>& index,
    const context::Processor&       lproc,
    const FieldHandler&             fdset ) -> mtc::api<IQuery>
  {
    auto  zterms( terms );

    if ( zterms.empty() )
      zterms = RankQueryTerms( LoadQueryTerms( query ), index, lproc );

    return RichBuilder( zterms, index, lproc, fdset )
      .BuildQuery( query, { fdset } ).query.ptr();
  }

}}
