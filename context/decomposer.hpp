# if !defined( __DelphiX_context_decomposer_hpp__ )
# define __DelphiX_context_decomposer_hpp__
# include "lang-api.hpp"
# include <functional>

namespace DelphiX {
namespace context {

  using Decomposer = std::function<void( IImage*,
    const Slice<const textAPI::TextToken>&,
    const Slice<const textAPI::MarkupTag>& )>;

  class DefaultDecomposer
  {
  public:
    void  operator()( IImage*,
      const Slice<const textAPI::TextToken>&,
      const Slice<const textAPI::MarkupTag>& );
  };

  auto  LoadDecomposer( const char*, const char* ) -> Decomposer;

}};

# endif // !__DelphiX_context_decomposer_hpp__
