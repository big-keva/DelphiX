# if !defined( __DelphiX_textAPI_word_break_hpp__ )
# define __DelphiX_textAPI_word_break_hpp__
# include "DOM-body.hpp"
# include "../slices.hpp"
# include <moonycode/chartype.h>

namespace DelphiX {
namespace textAPI {

  inline
  bool  IsPunct( widechar c )
  {
    return (codepages::charType[c] & 0xf0) == codepages::cat_P
        || (codepages::charType[c] & 0xf0) == codepages::cat_S;
  }

 /*
  * word breaker
  *
  * stores words with context flags, pointer, offset and length to output array
  */
  template <class Allocator>
  auto  BreakWords(
    BaseBody<Allocator>&          output,
    const Slice<const TextChunk>& blocks ) -> BaseBody<Allocator>&
  {
    uint32_t  offset = 0;

    for ( auto beg = blocks.data(), end = beg + blocks.size(); beg != end; offset += (beg++)->length )
    {
      auto  sblock = beg->GetWideStr();
      auto  buforg = output.AddBuffer( sblock.data(), sblock.size() );
      auto  ptrtop = buforg;
      auto  ptrend = ptrtop + sblock.size();

      while ( ptrtop != ptrend )
      {
        unsigned  uFlags = 0;

      // detect lower space
        if ( codepages::IsBlank( *ptrtop ) )
        {
          uFlags = TextToken::lt_space;

          for ( ++ptrtop; ptrtop != ptrend && codepages::IsBlank( *ptrtop ); ++ptrtop )
            (void)NULL;
        }

      // select next word
        if ( ptrtop != ptrend )
        {
          auto  origin = ptrtop;

        // get substring length
          if ( IsPunct( *ptrtop++ ) ) uFlags |= TextToken::is_punct;
            else
          while ( ptrtop != ptrend && !codepages::IsBlank( *ptrtop ) && !IsPunct( *ptrtop ) )
            ++ptrtop;

        // create word string
          output.GetTokens().push_back( { uFlags, origin,
            uint32_t(offset + origin - buforg), uint32_t(ptrtop - origin) } );
        }
      }
    }
    return output;
  }

  template <class Allocator>
  auto  CopyMarkup(
    BaseBody<Allocator>&          output,
    const Slice<const MarkupTag>& markup ) -> BaseBody<Allocator>&
  {
    using vectorType = typename std::remove_reference<decltype(output.GetMarkup())>::type;
    using formatType = typename vectorType::iterator;

    auto  fwdFmt = std::vector<formatType>();
    auto  bckFmt = std::vector<formatType>();

    output.GetMarkup().insert( output.GetMarkup().end(), markup.data(),
      markup.data() + markup.size() );

  // create formats indices
    for ( auto beg = output.GetMarkup().begin(); beg != output.GetMarkup().end(); ++beg )
    {
      fwdFmt.push_back( beg );
      bckFmt.push_back( beg );
    }
    std::sort( bckFmt.begin(), bckFmt.end(), []( formatType a, formatType b )
      {
        auto  rescmp = a->uUpper - b->uUpper;
        return rescmp != 0 ? rescmp < 0 : a->uLower < b->uLower;
      } );

  // open and close the items
    auto  fwdtop = fwdFmt.begin();
    auto  bcktop = bckFmt.begin();

    for ( size_t windex = 0; windex != output.GetTokens().size(); ++windex )
    {
      auto& rfword = output.GetTokens().at( windex );
      auto  uLower = rfword.offset;
      auto  uUpper = rfword.offset + rfword.length - 1;

      while ( fwdtop != fwdFmt.end() && (*fwdtop)->uLower <= uLower )
        (*fwdtop++)->uLower = windex;
      while ( bcktop != bckFmt.end() && (*bcktop)->uUpper <= uUpper )
        (*bcktop++)->uUpper = windex;
    }
    while ( fwdtop != fwdFmt.end() )
      (*fwdtop++)->uLower = output.GetTokens().size() - 1;
    while ( bcktop != bckFmt.end() )
      (*bcktop++)->uUpper = output.GetTokens().size() - 1;

    return output;
  }

  inline
  auto  BreakWords( const Slice<const TextChunk>& blocks ) -> Body
  {
    Body  body;

    return std::move( BreakWords( body, blocks ) );
  }

}}

# endif // !__DelphiX_textAPI_word_break_hpp__
