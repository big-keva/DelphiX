# if !defined( __DelphiX_context_make_image_hpp__ )
# define __DelphiX_context_make_image_hpp__
# include "../lang-api.hpp"

namespace DelphiX {
namespace context {

  struct Lite final
  {
    static  auto  Create() -> mtc::api<IImage>;
  };

  struct BM25 final
  {
    static  auto  Create() -> mtc::api<IImage>;
  };

  struct Rich final
  {
    static  auto  Create() -> mtc::api<IImage>;
  };

}}

# endif // !__DelphiX_context_make_image_hpp__
