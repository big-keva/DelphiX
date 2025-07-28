# if !defined( __DelphiX_lang_api_hpp__ )
# define __DelphiX_lang_api_hpp__
# include "contents.hpp"

namespace DelphiX {

  struct languageId final
  {
    constexpr static  unsigned russian   = 0;
    constexpr static  unsigned ukrainian = 1;
    constexpr static  unsigned english   = 2;

    constexpr static  unsigned hieroglyph = 0xff;
  };

  struct ILemmatizer: public mtc::Iface
  {
    struct IWord: public mtc::Iface
    {
      virtual void  AddTerm( uint32_t lex, float flp,
        const uint8_t*, size_t ) = 0;
      virtual void  AddStem( const widechar* pws, size_t len, uint32_t cls, float flp,
        const uint8_t*, size_t ) = 0;
    };

    virtual int   Lemmatize( IWord*, const widechar*, size_t ) = 0;
  };

  typedef int  (*CreateLemmatizer)( ILemmatizer**, const char* );

};

# endif // !__DelphiX_lang_api_hpp__
