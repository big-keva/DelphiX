# if !defined( __DelphiX_tests_toolbox_dirtool_h__ )
# define __DelphiX_tests_toolbox_dirtool_h__
# include <string>

void  RemoveFiles( const char* path );
bool  SearchFiles( const char* path ) ;

inline  void  RemoveFiles( const std::string& path )  {  return RemoveFiles( path.c_str() );  }
inline  bool  SearchFiles( const std::string& path )  {  return SearchFiles( path.c_str() );  }

# endif // !__DelphiX_tests_toolbox_dirtool_h__

