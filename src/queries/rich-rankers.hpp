# if !defined( __DelphiX_src_queries_rich_rankers_hpp__ )
# define __DelphiX_src_queries_rich_rankers_hpp__
# include "../context/formats.hpp"
# include <vector>

namespace DelphiX {
namespace queries {

  class RichRanker;

  class TermRanker
  {
    std::vector<unsigned>   tagOffset;
    std::vector<double>     fidWeight;

    TermRanker( const TermRanker& ) = delete;
  public:
    TermRanker() = default;
    TermRanker( TermRanker&& tr ):
      tagOffset( std::move( tr.tagOffset ) ),
      fidWeight( std::move( tr.fidWeight ) )  {}

    double  operator()( unsigned tag, uint8_t fid ) const;
    auto    GetRanker( const Slice<const textAPI::RankerTag>& ) const -> RichRanker;
  };

  class RichRanker
  {
    friend class TermRanker;

    const TermRanker&                 ranker;
    mutable const textAPI::RankerTag* fmttop;
    mutable const textAPI::RankerTag* fmtend;

  protected:
    RichRanker( const TermRanker& r, const Slice<const textAPI::RankerTag>& f ):
      ranker( r ),
      fmttop( f.data() ),
      fmtend( f.data() + f.size() ) {}

  public:
    double  operator()( unsigned pos, uint8_t fid ) const
    {
      while ( fmttop + 1 != fmtend && fmttop[1].uLower <= pos && fmttop[1].uUpper >= pos )
        ++fmttop;

      return ranker( fmttop->uLower <= pos && fmttop->uUpper >= pos ? fmttop->format : unsigned(-1), fid );
    }
  };

  // TermRanker inline implementation

  inline  auto  TermRanker::operator()( unsigned tag, uint8_t fid ) const -> double
  {
    if ( tag < tagOffset.size() && (tag = tagOffset[tag]) != unsigned(-1) )
      return fidWeight[tag + fid];
    return 0.2;
  }

  inline  auto  TermRanker::GetRanker( const Slice<const textAPI::RankerTag>& fmt ) const -> RichRanker
  {
    return { *this, fmt };
  }

}}

# endif   // !__DelphiX_src_queries_rich_rankers_hpp__
