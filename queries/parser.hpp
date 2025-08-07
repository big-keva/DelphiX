# if !defined( __DelphiX_queries_parser_hpp__ )
# define __DelphiX_queries_parser_hpp__
# include <moonycode/codes.h>
# include <mtc/zmap.h>

namespace DelphiX {
namespace queries {

  class ParseError: public std::invalid_argument {  using std::invalid_argument::invalid_argument;  };

  auto  ParseQuery( const widechar*, size_t = -1 ) -> mtc::zval;
  template <class Allocator>
  auto  ParseQuery( const std::basic_string<widechar, std::char_traits<widechar>, Allocator>& str ) -> mtc::zval
    {  return ParseQuery( str.c_str(), str.size() );  }
  inline
  auto  ParseQuery( const std::basic_string_view<widechar>& str ) -> mtc::zval
    {  return ParseQuery( str.data(), str.size() );  }

  inline
  auto  ParseQuery( unsigned codepage, const char* str, size_t len = -1 ) -> mtc::zval
    {  return ParseQuery( codepages::mbcstowide( codepage, str, len ) );  }
  template <class Allocator>
  auto  ParseQuery( const std::basic_string<char, std::char_traits<char>, Allocator>& str, unsigned codepage = codepages::codepage_utf8 ) -> mtc::zval
    {  return ParseQuery( codepages::mbcstowide( codepage, str ) );  }
  inline
  auto  ParseQuery( const std::string_view& str, unsigned codepage = codepages::codepage_utf8 ) -> mtc::zval
    {  return ParseQuery( codepages::mbcstowide( codepage, str.data(), str.size() ) );  }

}}

# endif   // !__DelphiX_queries_parser_hpp__
