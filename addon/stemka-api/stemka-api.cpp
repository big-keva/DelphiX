# include "../../lang-api.hpp"
# include "stemka/include/stemmers.h"
# include "stemka/include/fuzzyrus.h"
# include "stemka/include/fuzzyukr.h"
# include "moonycode/codes.h"

# define EXPORTED_API __attribute__((__visibility__("default")))

extern "C"
{
  int   EXPORTED_API  CreateLemmatizer( DelphiX::ILemmatizer**, const char* );

}

class Lemmatizer final: public DelphiX::ILemmatizer
{
  implement_lifetime_stub

public:
  int   Lemmatize( IWord*, const widechar*, size_t ) override;

};

/*
 * Lemmatize
 *
 * Creates text decomposition to lexemes for each word passed.
 * Has to be thread-safe, i.e. contain no read-write internal data.
 *
 */

stemScan  ruStemScan = { GetRusTables(), codepages::xlatOneToOne, GetRusVowels(),
  (unsigned)-1 };
stemScan  ukStemScan = { GetUkrTables(), codepages::xlatOneToOne, GetUkrVowels(),
  (unsigned)-1 };

bool  StematizeWord( DelphiX::ILemmatizer::IWord* pword, stemScan& scan, const widechar* pws, const char* psz, size_t len )
{
  unsigned  ustems[0x10];
  int       nstems = GetStemLenBuffer( &scan, ustems, std::size(ustems), psz, len );

  if ( nstems != 0 )
  {
    for ( int i = 0; i != nstems; ++i )
      pword->AddStem( pws, ustems[i], 1U, nullptr, 0 );
    return true;
  }
  return false;
}

int   Lemmatizer::Lemmatize( IWord* pterms, const widechar* pwsstr, size_t cchstr )
{
  char      szword[0x40];    // no longer words in russian and ukrainian
  char*     pszout = szword;
  widechar  wsword[0x40];
  widechar* pwsout = wsword;

  if ( cchstr >= 0x40 )
    return EOVERFLOW;

  for ( auto src = pwsstr, end = src + cchstr; src != end; ++src )
    *pszout++ = codepages::__impl__::utf_1251<>::translate( *pwsout++ = codepages::xlatUtf16Lower[*src] );

  // try create bases for two languages
  return StematizeWord( pterms, ruStemScan, wsword, szword, pszout - szword ), 0;
}
