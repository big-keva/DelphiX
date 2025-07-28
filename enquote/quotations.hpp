# if !defined( __DelphiX_enquote_quotations_hpp__ )
# define __DelphiX_enquote_quotations_hpp__
# include "../text-api.hpp"
# include "../slices.hpp"
# include <functional>

namespace DelphiX {
namespace enquote {

  using QuotesFunction = std::function<void(textAPI::IText*, const Slice<const char>&)>;

  void  AsListOfQuotes( textAPI::IText*, const Slice<const char>& );
  void  AsMarkedQuotes( textAPI::IText*, const Slice<const char>& );
  void  AsOriginalText( textAPI::IText*, const Slice<const char>& );

}}

# endif   // !__DelphiX_enquote_quotations_hpp__
