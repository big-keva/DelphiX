# include "base-queries.hpp"

namespace DelphiX {
namespace queries {

  // SimpleQuery implementation

  uint32_t  SimpleQuery::SearchDoc( uint32_t findId )
  {
    while ( entryRef.uEntity < findId )
      entryRef = entities->Find( findId );
    return entryRef.uEntity;
  }

  // SpreadQuery implementation

  void  SpreadQuery::AddQuery( mtc::api<IQuery> query, double range )
  {
    double  rgsumm = 0.0;

    spread.push_back( { range, 0.0, unsigned(spread.size()), query } );

    std::sort( spread.begin(), spread.end(), []( const SubQuery& s1, const SubQuery& s2 )
      {  return s1.fRange > s2.fRange; } );

    for ( auto& next : spread )
      rgsumm += next.fRange;

    for ( auto& next : spread )
      next.fLeast = (rgsumm -= next.fRange);
  }

  // GetMatchAll implementation

  uint32_t  GetMatchAll::SearchDoc( uint32_t findId )
  {
    uint32_t uFound = 0;
    uint32_t nextId;

    if ( (findId = std::max( findId, uDocId )) == uint32_t(-1) )
      return uDocId;

    for ( auto next = spread.begin(); next != spread.end(); )
    {
      if ( (nextId = next->SearchDoc( findId )) == uint32_t(-1) )
        return uDocId = uint32_t(-1);

      if ( next == spread.begin() || nextId == uFound ) ++next;
        else {  findId = nextId;  next = spread.begin();  }
      uFound = nextId;
    }
    return uDocId = uFound;
  }

  // GetMatchAny implementation

  uint32_t  GetMatchAny::SearchDoc( uint32_t findId )
  {
    uint32_t uFound = uint32_t(-1);

    if ( (findId = std::max( findId, uDocId )) == uint32_t(-1) )
      return uDocId;

    for ( auto& next: spread )
      uFound = std::min( uFound, next.SearchDoc( findId ) );

    return uDocId = uFound;
  }

  // GetByQuorum implementation

  uint32_t  GetByQuorum::SearchDoc( uint32_t findId )
  {
    auto      getone = spread.begin();
    uint32_t  uFound;
    double    flSumm;
    auto      reinit = [&]()
      {
        getone = spread.begin();
        ++findId;
        flSumm = 0.0;
        uFound = 0;
      };

    if ( (findId = std::max( findId, uDocId )) == uint32_t(-1) )
      return uDocId;

    for ( reinit(); ; )
    {
      uint32_t  nextId;

      // check if any chance to find document with current state and further
      // if not, go to next search item
      if ( flSumm + getone->fLeast < quorum )
        {  reinit();  continue;  }

      // if not found, continue to next element
      if ( (nextId = getone->SearchDoc( findId )) != uint32_t(-1) )
      {
        if ( nextId == uFound || getone == spread.begin() )
        {
          flSumm += getone->fRange;
          uFound = nextId;
        }
      }
        else
      {
        if ( getone->fLeast >= quorum )
          return uDocId = uint32_t(-1);
      }

      if ( ++getone == spread.end() )
      {
        if ( flSumm >= quorum )
          return uDocId = uFound;
        if ( uFound == 0 )
          return uDocId = uint32_t(-1);
        reinit();
      }
    }
  }

}}
