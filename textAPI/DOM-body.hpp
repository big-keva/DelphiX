# if !defined( __DelphiX_textAPI_DOM_body_hpp__ )
# define __DelphiX_textAPI_DOM_body_hpp__
# include "text-api.hpp"

namespace DelphiX {
namespace textAPI {

  template <class Allocator>
  class BaseBody
  {
  public:
    BaseBody( Allocator = Allocator() );
    BaseBody( BaseBody&& );
   ~BaseBody();

  public:
    auto  GetMarkup() const -> const std::vector<MarkupTag, Allocator>&  {  return markup;  }
    auto  GetTokens() const -> const std::vector<TextToken, Allocator>&  {  return tokens;  }

    auto  AddBuffer( const widechar*, size_t ) -> const widechar*;
    auto  GetMarkup() -> std::vector<MarkupTag, Allocator>&  {  return markup;  }
    auto  GetTokens() -> std::vector<TextToken, Allocator>&  {  return tokens;  }

    auto  Serialize( IText* ) -> IText*;

  protected:
    std::vector<MarkupTag, Allocator> markup;
    std::vector<TextToken, Allocator> tokens;
    std::vector<widechar*, Allocator> bufbox;

  };

  using Body = BaseBody<std::allocator<char>>;

  // BaseBody template implementation

  template <class Allocator>
  BaseBody<Allocator>::BaseBody( Allocator a ):
    markup( a ),
    tokens( a ),
    bufbox( a )
  {}

  template <class Allocator>
  BaseBody<Allocator>::BaseBody( BaseBody&& b ):
    markup( std::move( b.markup ) ),
    tokens( std::move( b.tokens ) ),
    bufbox( std::move( b.bufbox ) )
  {}

  template <class Allocator>
  BaseBody<Allocator>::~BaseBody()
  {
    auto  deallocator = typename std::allocator_traits<Allocator>::template rebind_alloc<widechar>(
      bufbox.get_allocator() );

    for ( auto next: bufbox )
      deallocator.deallocate( next, 0 );
  }

  template <class Allocator>
  auto  BaseBody<Allocator>::AddBuffer( const widechar* pws, size_t len ) -> const widechar*
  {
    auto  palloc = typename std::allocator_traits<Allocator>::template rebind_alloc<widechar>(
      bufbox.get_allocator() ).allocate( len );

    bufbox.push_back( (widechar*)memcpy(
      palloc, pws, len * sizeof(widechar) ) );

    return bufbox.back();
  }

  template <class Allocator>
  auto  BaseBody<Allocator>::Serialize( IText* text ) -> IText*
  {
    auto  lineIt = tokens.begin();
    auto  markIt = markup.begin();
    auto  fPrint = std::function<void( IText*, uint32_t )>( [&]( IText* to, uint32_t up )
      {
        while ( lineIt != tokens.end() && lineIt - tokens.begin() <= up )
        {
        // check if print next line to current IText*
          if ( markIt == markup.end() || (lineIt - tokens.begin()) < markIt->uLower )
          {
            to->AddWideStr( lineIt->pwsstr, lineIt->length );

            if ( ++lineIt == tokens.end() ) return;
              continue;
          }

        // check if open new span
          if ( lineIt - tokens.begin() >= markIt->uLower )
          {
            auto  new_to = to->AddTextTag( markIt->format );
            auto  uUpper = markIt->uUpper;
              ++markIt;
            fPrint( new_to.ptr(), uUpper );
          }
        }
      } );

    fPrint( text, -1 );

    return text;
  }

}}

# endif   // !__DelphiX_textAPI_DOM_body_hpp__
