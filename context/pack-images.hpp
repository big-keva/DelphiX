# if !defined( __DelphiX_context_pack_images_hpp__ )
# define __DelphiX_context_pack_images_hpp__
# include "../context/text-image.hpp"
# include <mtc/iStream.h>
# include <functional>

namespace DelphiX {
namespace context {
namespace imaging {

  void  Pack( std::function<void(const void*, size_t)>, const mtc::span<const TextToken>& );
  void  Pack( mtc::IByteStream*, const mtc::span<const TextToken>& );
  auto  Pack( const mtc::span<const TextToken>& ) -> std::vector<char>;

  void  Unpack(
    std::function<void(unsigned, const mtc::span<const widechar>&)> addstr,
    std::function<void(unsigned, unsigned)>                         addref,
    const mtc::span<const char>& );

  template <class Allocator>
  auto  Unpack( BaseImage<Allocator>& image,
    const mtc::span<const char>& input ) -> context::BaseImage<Allocator>&
  {
    Unpack( [&]( unsigned uflags, const mtc::span<const widechar>& inp )
      {
        image.GetTokens().push_back( { uflags, image.AddBuffer( inp.data(), inp.size() ),
          unsigned(image.GetTokens().size()), unsigned(inp.size()) } );
      },
      [&]( unsigned uflags, unsigned pos )
      {
        if ( pos >= image.GetTokens().size() )
          throw std::invalid_argument( "broken text image - invalid reference" );
        image.GetTokens().push_back( image.GetTokens()[pos] );
        image.GetTokens().back().uFlags = uflags;
      }, input );
    return image;
  }
  auto  Unpack( const mtc::span<const char>& ) -> context::Image;

}}}

# endif   // !__DelphiX_context_pack_images_hpp__
