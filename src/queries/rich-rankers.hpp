# if !defined( __DelphiX_src_queries_rich_rankers_hpp__ )
# define __DelphiX_src_queries_rich_rankers_hpp__
# include "../../context/pack-format.hpp"
# include "field-set.hpp"
# include <vector>

namespace DelphiX {
namespace queries {

  class RichRanker;

  class TermRanker
  {
    std::vector<unsigned>   tagOffset;
    std::vector<double>     fidWeight;

    TermRanker( const TermRanker& ) = delete;
    TermRanker& operator=( const TermRanker& ) = delete;
  public:
    TermRanker() = default;
    TermRanker( TermRanker&& tr ):
      tagOffset( std::move( tr.tagOffset ) ),
      fidWeight( std::move( tr.fidWeight ) )  {}
    TermRanker( const FieldSet&, const context::Lexeme&, double, bool strict );

  // rank
    double  operator()( unsigned tag, uint8_t fid ) const;
    auto    GetRanker( const mtc::span<const context::RankerTag>& ) const -> RichRanker;
  };

  class RichRanker
  {
    friend class TermRanker;

    const TermRanker&                 ranker;
    mutable const context::RankerTag* fmttop;
    mutable const context::RankerTag* fmtend;

  protected:
    RichRanker( const TermRanker& r, const mtc::span<const context::RankerTag>& f ):
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

  TermRanker::TermRanker( const FieldSet& fds, const context::Lexeme& lex, double flo, bool fmatch )
  {
    for ( auto tag: fds )
    {
    // get tag relocation
      if ( tagOffset.size() <= tag )
        tagOffset.insert( tagOffset.end(), tag - tagOffset.size() + 1, uint32_t(-1) );
      if ( tagOffset[tag] == uint32_t(-1) )
        tagOffset[tag] = unsigned(fidWeight.size());

    // insert forms mapping
      if ( fidWeight.size() <= tagOffset[tag] + 256 )
        fidWeight.insert( fidWeight.end(), tagOffset[tag] + 256 - fidWeight.size(), -1.0 );

    // fill default value
      if ( fmatch )
        std::fill( fidWeight.begin() + tagOffset[tag], fidWeight.begin() + tagOffset[tag] + 256, -1 );
      else
        std::fill( fidWeight.begin() + tagOffset[tag], fidWeight.begin() + tagOffset[tag] + 256, 0.5 * flo );

      if ( lex.GetForms().empty() )
      {
        fidWeight[255 + tagOffset[tag]] = flo;
      }
        else
      {
        for ( auto fid: lex.GetForms() )
          fidWeight[fid + tagOffset[tag]] = flo;
      }
    }
  }

  inline  auto  TermRanker::operator()( unsigned tag, uint8_t fid ) const -> double
  {
    if ( tag < tagOffset.size() && (tag = tagOffset[tag]) != unsigned(-1) )
      return fidWeight[tag + fid];
    return 0.2;
  }

  inline  auto  TermRanker::GetRanker( const mtc::span<const context::RankerTag>& fmt ) const -> RichRanker
  {
    return { *this, fmt };
  }

}}

# endif   // !__DelphiX_src_queries_rich_rankers_hpp__
