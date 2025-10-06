# if !defined( __DelphiX_context_pack_format_hpp__ )
# define __DelphiX_context_pack_format_hpp__
# include "../text-api.hpp"
# include "../fields.hpp"
# include <mtc/iStream.h>
# include <functional>

namespace DelphiX {
namespace context {
namespace formats {

  template <class T>
  using FormatTag = textAPI::FormatTag<T>;

  void  Pack( std::function<void(const void*, size_t)>, const Slice<const FormatTag<unsigned>>& );
  void  Pack( std::function<void(const void*, size_t)>, const Slice<const FormatTag<const char*>>&, FieldHandler& );

  void  Pack( mtc::IByteStream*, const Slice<const FormatTag<unsigned>>& );
  void  Pack( mtc::IByteStream*, const Slice<const FormatTag<const char*>>&, FieldHandler& );

  auto  Pack( const Slice<const FormatTag<unsigned>>& ) -> std::vector<char>;
  auto  Pack( const Slice<const FormatTag<const char*>>&, FieldHandler& ) -> std::vector<char>;

  auto  Unpack( FormatTag<unsigned>*, FormatTag<unsigned>*, const char*, const char* ) -> size_t;
  void  Unpack( std::function<void(const FormatTag<unsigned>&)>, const char*, const char* );

  inline  auto  Unpack( FormatTag<unsigned>*  out, size_t  max, const char* src, size_t  len ) -> size_t
    {  return Unpack( out, out + max, src, src + len );  }

  inline  void  Unpack( std::function<void(const FormatTag<unsigned>&)> fn, const char* src, size_t  len )
    {  return Unpack( fn, src, src + len );  }

  inline  void  Unpack( std::function<void(const FormatTag<unsigned>&)> fn, const Slice<const char>& src )
    {  return Unpack( fn, src.data(), src.size() );  }

  template <size_t N>
  auto  Unpack( FormatTag<unsigned> (&tbeg)[N], const char* src, const char* end ) -> size_t
    {  return Unpack( tbeg, tbeg + N, src, end );  }

  template <size_t N>
  auto  Unpack( FormatTag<unsigned> (&tbeg)[N], const char* src, size_t  len ) -> size_t
    {  return Unpack( tbeg, tbeg + N, src, src + len );  }

  template <size_t N>
  auto  Unpack( FormatTag<unsigned> (&tbeg)[N], const Slice<const char>& src ) -> size_t
  {
    return Unpack( tbeg, tbeg + N, src.data(), src.size() );
  }

  auto  Unpack( const Slice<const char>& ) -> std::vector<FormatTag<unsigned>>;

}}}

# endif   // !__DelphiX_context_pack_format_hpp__
