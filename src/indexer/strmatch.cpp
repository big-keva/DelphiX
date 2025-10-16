# include "strmatch.hpp"
# include "mtc/utf.hpp"

namespace DelphiX {
namespace indexer {

  inline  size_t  cbchar( const uint8_t* str, const uint8_t* end )
  {
    return std::max( mtc::utf::cbchar( (const char*)str, end - str ), (size_t)1 );
  }

  int   strmatch( const uint8_t* sbeg, const uint8_t* send, const uint8_t* mbeg, const uint8_t* mend )
  {
    int   rcmp;

    while ( mbeg != mend )
    {
      auto  next = *mbeg++;

      if ( next == '?' )
        return sbeg < send && strmatch( sbeg + cbchar( sbeg, send ), send, mbeg, mend ) == 0 ? 0 : -1;

      if ( next == '*' )
      {
        for ( auto skip = 0; sbeg + skip <= send; ++skip )
          if ( strmatch( sbeg + skip, send, mbeg, mend ) == 0 )
            return 0;
        return -1;
      }

      if ( sbeg >= send )
        return -1;

      if ( (rcmp = *sbeg - next) == 0 ) ++sbeg;
        else return rcmp;
    }

    return sbeg == send ? 0 : 1;
  }

  int   strmatch( const mtc::radix::key& key, const std::string& tpl )
  {
    return strmatch( key.data(), key.data() + key.size(), (const uint8_t*)tpl.data(),
      (const uint8_t*)tpl.data() + tpl.size() );
  }

}}
