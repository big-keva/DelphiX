# if !defined( __DelphiX_context_pack_format_hpp__ )
# define __DelphiX_context_pack_format_hpp__
# include "ranker-tag.hpp"
# include "../fields.hpp"
# include <mtc/span.hpp>
# include <mtc/iStream.h>
# include <functional>
# include <DeliriX/text-API.hpp>

namespace DelphiX {
namespace context {
namespace formats {

  using MarkupTag = DeliriX::MarkupTag;
  using RankerTag = context::RankerTag;

  void  Pack( std::function<void(const void*, size_t)>, const mtc::span<const RankerTag>& );
  void  Pack( std::function<void(const void*, size_t)>, const mtc::span<const MarkupTag>&, FieldHandler& );

  void  Pack( mtc::IByteStream*, const mtc::span<const RankerTag>& );
  void  Pack( mtc::IByteStream*, const mtc::span<const MarkupTag>&, FieldHandler& );

  auto  Pack( const mtc::span<const RankerTag>& ) -> std::vector<char>;
  auto  Pack( const mtc::span<const MarkupTag>&, FieldHandler& ) -> std::vector<char>;

  auto  Unpack( RankerTag*, RankerTag*, const char*, const char* ) -> size_t;
  void  Unpack( std::function<void(const RankerTag&)>, const char*, const char* );

  inline  auto  Unpack( RankerTag*  out, size_t  max, const char* src, size_t  len ) -> size_t
    {  return Unpack( out, out + max, src, src + len );  }

  inline  void  Unpack( std::function<void(const RankerTag&)> fn, const char* src, size_t  len )
    {  return Unpack( fn, src, src + len );  }

  inline  void  Unpack( std::function<void(const RankerTag&)> fn, const mtc::span<const char>& src )
    {  return Unpack( fn, src.data(), src.size() );  }

  template <size_t N>
  auto  Unpack( RankerTag (&tbeg)[N], const char* src, const char* end ) -> size_t
    {  return Unpack( tbeg, tbeg + N, src, end );  }

  template <size_t N>
  auto  Unpack( RankerTag (&tbeg)[N], const char* src, size_t  len ) -> size_t
    {  return Unpack( tbeg, tbeg + N, src, src + len );  }

  template <size_t N>
  auto  Unpack( RankerTag (&tbeg)[N], const mtc::span<const char>& src ) -> size_t
  {
    return Unpack( tbeg, tbeg + N, src.data(), src.size() );
  }

  auto  Unpack( const mtc::span<const char>& ) -> std::vector<RankerTag>;

}}}

# endif   // !__DelphiX_context_pack_format_hpp__
