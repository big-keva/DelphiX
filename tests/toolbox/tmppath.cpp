# include "tmppath.h"

# if defined( _WIN32 ) || defined( _WIN64 )
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>

auto  GetTmpPath() -> std::string
{
  char  tmp_path[1024];

  GetTempPath( sizeof(tmp_path), tmp_path );

  return tmp_path;
}


# else

auto  GetTmpPath() -> std::string {  return "/tmp/";  }

# endif
