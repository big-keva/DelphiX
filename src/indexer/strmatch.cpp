# include "strmatch.hpp"

namespace DelphiX {
namespace indexer {

  int   strmatch( const char* sbeg, const char* send, const char* mbeg, const char* mend )
  {
    int   rescmp = 0;

    while ( sbeg != send && mbeg != mend && (rescmp = uint8_t(*sbeg) - uint8_t(*mbeg)) == 0 && *sbeg != '*' && *sbeg != '?' )
      ++sbeg, ++mbeg;

    if ( sbeg == send )
      return 0 - (mbeg != mend);
    if ( mbeg == mend )
      return 1;
    if ( *sbeg == '?' )
      return 0 - (strmatch( sbeg + 1, send, mbeg + 1, mend ) == 0);
    if ( *sbeg == '*' )
    {
      if ( strmatch( sbeg + 1, send, mbeg, mend ) == 0 )
        return 0;
      if ( strmatch( sbeg, send, mbeg + 1, mend ) == 0 )
        return 0;
      return -1;
    }
    return rescmp;
  }

  int   strmatch( const std::string& str, const mtc::radix::key& match )
  {
    return strmatch( str.data(), str.data() + str.size(), (const char*)match.data(),
      (const char*)match.data() + match.size() );
  }

}}
