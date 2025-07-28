# if !defined( __DelphiX_context_lemmatiser_hpp__ )
# define __DelphiX_context_lemmatiser_hpp__
# include "../lang-api.hpp"

namespace DelphiX {
namespace context {

  auto  LoadLemmatizer( const char*, const char* ) -> mtc::api<ILemmatizer>;
  auto  MiniLemmatizer() -> mtc::api<ILemmatizer>;
  auto  MaxiLemmatizer() -> mtc::api<ILemmatizer>;

}};

# endif // !__DelphiX_context_lemmatiser_hpp__
