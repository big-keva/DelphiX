# if !defined( __DelphiX_compat_hpp__ )
# define __DelphiX_compat_hpp__

# if !defined( LINE_STRING )
#   define __LN_STRING( arg )  #arg
#   define _LN__STRING( arg )  __LN_STRING( arg )
#   define LINE_STRING _LN__STRING(__LINE__)
# endif   // !LINE_STRING

# if defined( _MSC_VER )
#   define pthread_self( ... )
#   define pthread_setname_np( ... )
# endif   // _MSC_VER

# if defined( _WIN32 ) || defined( _WIN64 )
#   include <io.h>
#   define open _open
#   define write _write
#   define close _close
# else
#   include <unistd.h>
# endif

  template <class A, class B>
  using AllocatorCast = typename std::allocator_traits<A>::template rebind_alloc<B>;

# endif   // !__DelphiX_compat_hpp__
