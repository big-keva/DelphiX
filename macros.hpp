# if !defined( __DelphiX_macros_hpp__ )
# define __DelphiX_macros_hpp__

# if !defined( LINE_STRING )
#   define __LN_STRING( arg )  #arg
#   define _LN__STRING( arg )  __LN_STRING( arg )
#   define LINE_STRING _LN__STRING(__LINE__)
# endif   // !LINE_STRING

# endif   // !__DelphiX_macros_hpp__
