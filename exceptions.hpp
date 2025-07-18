# if !defined( __DelphiX_exceptions_hpp__ )
# define __DelphiX_exceptions_hpp__
# include <stdexcept>

namespace DelphiX {

  class index_overflow: public std::runtime_error {  using runtime_error::runtime_error;  };

}

# endif   // __DelphiX_exceptions_hpp__
