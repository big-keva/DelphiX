# if !defined( __DelphiX_context_x_contents_hpp__ )
# define __DelphiX_context_x_contents_hpp__
# include "../contents.hpp"
# include "text-image.hpp"
# include "fields-man.hpp"

namespace DelphiX {
namespace context {

  auto  GetMiniContents(
    const mtc::span<const mtc::span<const Lexeme>>&,
    const mtc::span<const DeliriX::MarkupTag>&, FieldHandler& ) -> mtc::api<IContents>;
  auto  GetBM25Contents(
    const mtc::span<const mtc::span<const Lexeme>>&,
    const mtc::span<const DeliriX::MarkupTag>&, FieldHandler& ) -> mtc::api<IContents>;
  auto  GetRichContents(
    const mtc::span<const mtc::span<const Lexeme>>&,
    const mtc::span<const DeliriX::MarkupTag>&, FieldHandler& ) -> mtc::api<IContents>;

}}

# endif   // !__DelphiX_context_x_contents_hpp__
