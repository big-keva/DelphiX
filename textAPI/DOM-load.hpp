# if !defined( __DelphiX_textAPI_DOM_load_hpp__ )
# define __DelphiX_textAPI_DOM_load_hpp__
# include "text-api.hpp"
# include <mtc/serialize.h>
# include <functional>
# include <stdexcept>
# include <memory>

namespace DelphiX {
namespace textAPI {
namespace load_as {

  class ParseError : public std::runtime_error
    {  using std::runtime_error::runtime_error;  };

  void  Tags( mtc::api<IText>, std::function<char()> );
  void  Json( mtc::api<IText>, std::function<char()> );

  template <class S>
  auto  MakeSource( S* s ) -> std::function<char()>
  {
    return [source = std::make_shared<S*>( s )]() mutable -> char
      {
        char  c;

        if ( (*source = ::FetchFrom( *source, c )) == nullptr )
          throw std::runtime_error( "::FetchFrom() failed" );
        return c;
      };
  }

}}}

# endif // !__DelphiX_textAPI_DOM_load_hpp__
