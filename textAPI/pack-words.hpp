# if !defined( __DelphiX_textAPI_pack_words_hpp__ )
# define __DelphiX_textAPI_pack_words_hpp__
/*# include "DOM-body.hpp"
# include "../slices.hpp"
# include <mtc/iStream.h>
# include <functional>

namespace DelphiX {
namespace textAPI {

  void  PackWords( mtc::IByteStream*, const Slice<const TextToken>& );
  void  PackWords( std::function<void(const void*, size_t)>, const Slice<const TextToken>& );
  auto  PackWords( const Slice<const TextToken>& ) -> std::vector<char>;

  void  UnpackWords(
    std::function<void(unsigned, const Slice<const widechar>&)>,
    std::function<void(unsigned, unsigned)>, const Slice<const char>& );
  auto  UnpackWords( const Slice<const char>& ) -> Body;
}}
*/

# endif   // !__DelphiX_textAPI_pack_words_hpp__
