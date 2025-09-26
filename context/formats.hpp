# if !defined( __DelphiX_context_formats_hpp__ )
# define __DelphiX_context_formats_hpp__
# include "../text-api.hpp"
# include "../fields.hpp"
# include <mtc/iStream.h>
# include <functional>

namespace DelphiX {
namespace context {
namespace formats {

  template <class T>
  using FormatTag = textAPI::FormatTag<T>;

  void  Pack( std::function<void(const void*, size_t)>,
    const Slice<const FormatTag<unsigned>>& );
  void  Pack( std::function<void(const void*, size_t)>,
    const Slice<const FormatTag<const char*>>&, FieldHandler& );

  void  Pack( mtc::IByteStream*,
    const Slice<const FormatTag<unsigned>>& );
  void  Pack( mtc::IByteStream*,
    const Slice<const FormatTag<const char*>>&, FieldHandler& );

  auto  Pack(
    const Slice<const FormatTag<unsigned>>& ) -> std::vector<char>;
  auto  Pack(
    const Slice<const FormatTag<const char*>>&, FieldHandler& ) -> std::vector<char>;

  auto  Unpack(
    FormatTag<unsigned>*  tbeg, FormatTag<unsigned>*  tend,
    const char*           pbeg, const char*           pend ) -> size_t;

  inline
  auto  Unpack(
    FormatTag<unsigned>*  tbeg, size_t  outl,
    const char*           pbeg, size_t  srcl ) -> size_t
  {
    return Unpack( tbeg, tbeg + outl, pbeg, pbeg + srcl );
  }

  void  Unpack( std::function<void(const FormatTag<unsigned>&)>,
    const char* pbeg, const char* pend );

  inline
  void  Unpack( std::function<void(const FormatTag<unsigned>&)> fn,
    const char* pbeg, size_t  srcl )
  {
    return Unpack( fn, pbeg, pbeg + srcl );
  }

  template <size_t N>
  auto  Unpack( FormatTag<unsigned> (&tbeg)[N],
    const char* pbeg, const char* pend ) -> size_t
  {
    return Unpack( tbeg, tbeg + N, pbeg, pend );
  }

  template <size_t N>
  auto  Unpack( FormatTag<unsigned> (&tbeg)[N],
    const char* pbeg, size_t  srcl ) -> size_t
  {
    return Unpack( tbeg, tbeg + N, pbeg, pbeg + srcl );
  }

  template <size_t N>
  auto  Unpack(
    FormatTag<unsigned> (&tbeg)[N], const Slice<const char>& data ) -> size_t
  {
    return Unpack( tbeg, tbeg + N, data.data(), data.size() );
  }

  auto  Unpack( const Slice<const char>& ) -> std::vector<FormatTag<unsigned>>;

}}}

# endif   // !__DelphiX_context_formats_hpp__
