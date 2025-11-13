# include "dirtool.h"
# include <mtc/directory.h>
# if defined( _MSC_VER )
#   include <io.h>
# else
#   include <unistd.h>
# endif

void  RemoveFiles( const char* path )
{
  auto  dir = mtc::directory::Open( path );

  if ( dir.defined() )
    for ( auto entry = dir.Get(); entry.defined(); entry = dir.Get() )
      remove( mtc::strprintf( "%s%s", entry.folder(), entry.string() ).c_str() );
}

bool  SearchFiles( const char* path )
{
  auto  dir = mtc::directory::Open( path );

  return dir.defined() && dir.Get().defined();
}
