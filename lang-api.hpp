# if !defined( __DelphiX_lang_api_hpp__ )
# define __DelphiX_lang_api_hpp__
# include "contents.hpp"
# include "text-api.hpp"

namespace DelphiX {

  struct languageId final
  {
    constexpr static  unsigned russian   = 0;
    constexpr static  unsigned ukrainian = 1;
    constexpr static  unsigned english   = 2;

    constexpr static  unsigned hieroglyph = 0xff;
  };

  struct IImage: public IContents
  {
    virtual void  AddTerm(
      unsigned        pos,
      unsigned        idl,
      uint32_t        lex, const uint8_t*, size_t ) = 0;
    virtual void  AddStem(
      unsigned        pos,
      unsigned        idl,
      const widechar* str,
      size_t          len,
      uint32_t        cls, const uint8_t*, size_t ) = 0;
  };

  struct DecomposeHolder;

  typedef int (*DecomposeInit)( DecomposeHolder**, const char* );
  typedef int (*DecomposeFree)( DecomposeHolder* );

 /*
  * DecomposeText
  *
  * Creates text decomposition to lexemes for each word passed.
  * Has to be thread-safe, i.e. contain no read-write internal data.
  *
  */
  typedef int (*DecomposeText)( DecomposeHolder*, IImage*,
    const textAPI::TextToken*, size_t,
    const textAPI::MarkupTag*, size_t );

};

# endif // !__DelphiX_lang_api_hpp__
