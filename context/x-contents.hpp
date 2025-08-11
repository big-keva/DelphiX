# if !defined( __DelphiX_context_x_contents_hpp__ )
# define __DelphiX_context_x_contents_hpp__
# include "../contents.hpp"
# include "text-image.hpp"
# include "fields-man.hpp"

namespace DelphiX {
namespace context {

  auto  GetMiniContents(
    const Slice<const Slice<const Lexeme>>&,
    const Slice<const textAPI::MarkupTag>&, FieldHandler& ) -> mtc::api<IContents>;
  auto  GetBM25Contents(
    const Slice<const Slice<const Lexeme>>&,
    const Slice<const textAPI::MarkupTag>&, FieldHandler& ) -> mtc::api<IContents>;
  auto  GetRichContents(
    const Slice<const Slice<const Lexeme>>&,
    const Slice<const textAPI::MarkupTag>&, FieldHandler& ) -> mtc::api<IContents>;

}}

# endif   // !__DelphiX_context_x_contents_hpp__
