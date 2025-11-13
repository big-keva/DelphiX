# if !defined( __DelphiX_context_ranker_tag_hpp__ )
# define __DelphiX_context_ranker_tag_hpp__
# include <moonycode/codes.h>
# include <mtc/interfaces.h>
# include <mtc/serialize.h>
# include <stdexcept>
# include <cstdint>

namespace DelphiX {
namespace context {

  struct RankerTag
  {
    unsigned    format;
    uint32_t    uLower;     // start offset, bytes
    uint32_t    uUpper;     // end offset, bytes

  public:
    bool  operator==( const RankerTag& to ) const
      {  return format == to.format && uLower == to.uLower && uUpper == to.uUpper;  }
    bool  operator!=( const RankerTag& to ) const
      {  return !(*this == to);  }
    bool  Covers( uint32_t pos ) const
      {  return pos >= uLower && pos <= uUpper;  }
    bool  Covers( const std::pair<uint32_t, uint32_t>& span ) const
      {  return uLower <= span.first && uUpper >= span.second;  }
  };

}}

# endif // !__DelphiX_context_ranker_tag_hpp__
