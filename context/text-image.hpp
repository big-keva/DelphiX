# if !defined( __DelphiX_context_text_image_hpp__ )
# define __DelphiX_context_text_image_hpp__
# include "index-keys.hpp"
# include "DeliriX/text-API.hpp"
# include "../compat.hpp"
# include <mtc/bitset.h>
# include <functional>
# include <memory>

namespace DelphiX {
namespace context {

  struct TextToken
  {
    enum: unsigned
    {
      lt_space = 1,
      is_punct = 2,
      is_first = 4
    };

    unsigned        uFlags;
    const widechar* pwsstr;
    uint32_t        offset;
    uint32_t        length;

    auto  GetWideStr() const -> std::basic_string_view<widechar>  {  return { pwsstr, length };  }
    auto  LeftSpaced() const {  return (uFlags & lt_space) != 0;  }
    auto  IsPointing() const {  return (uFlags & is_punct) != 0;  }
    auto  IsFirstOne() const {  return (uFlags & is_first) != 0;  }

  };

  class Lexeme: public Key
  {
    using Key::Key;

    class Forms
    {
      uint64_t  fids[4] = { 0UL, 0UL, 0UL, 0UL };

      class const_iterator
      {
        const uint64_t* fidPtr;
        unsigned        uOrder;
        unsigned        uShift;

      public:
        const_iterator( const const_iterator& it ):
          fidPtr( it.fidPtr ),
          uOrder( it.uOrder ),
          uShift( it.uShift ) {}
        const_iterator( const uint64_t* p, unsigned u, unsigned o ):
          fidPtr( p ),
          uOrder( u ),
          uShift( o ) {}
        bool  operator == ( const const_iterator& it ) const
          {  return fidPtr == it.fidPtr && uOrder == it.uOrder && uShift == it.uShift;  }
        bool  operator != ( const const_iterator& it ) const
          {  return fidPtr != it.fidPtr || uOrder != it.uOrder || uShift != it.uShift;  }
        auto  operator *() const -> uint8_t
          {  return uint8_t(uOrder * sizeof(uint64_t) * CHAR_BIT + uShift);  }
        auto  operator ++() -> const_iterator&
          {
            if ( ++uShift == sizeof(uint64_t) * CHAR_BIT )
              {  ++uOrder;  uShift = 0;  }

            while ( uOrder < 4 )
            {
              while ( uShift < sizeof(uint64_t) * CHAR_BIT && (fidPtr[uOrder] & (uint64_t(1) << uShift)) == 0 )
                ++uShift;
              if ( uShift == sizeof(uint64_t) * CHAR_BIT )
                {  ++uOrder;  uShift = 0;  }
            }
            return *this;
          }
      };

    public:
      bool    empty() const
        {  return mtc::bitset_empty( fids );  }
      size_t  size() const
        {  return mtc::bitset_count( fids );  }
      void    set( uint8_t fid )
        {  return mtc::bitset_set( fids, fid );  }
      void    set( const uint8_t* fid, size_t len )
        {  for ( auto end = fid + len; fid != end; ++fid )  set( *fid );  }
      auto  front() const -> uint8_t
        {  return mtc::bitset_first( fids );  }
      auto  back() const -> uint8_t
        {  return mtc::bitset_last( fids );  }
      auto  begin() const -> const_iterator
        {
          auto  ifirst = mtc::bitset_first( fids );
          auto  uorder = unsigned(ifirst / (sizeof(uint64_t) * CHAR_BIT));
          auto  ushift = unsigned(ifirst % (sizeof(uint64_t) * CHAR_BIT));

          return ifirst != -1 ? const_iterator( fids, uorder, ushift ) : end();
        }
      auto  end() const -> const_iterator
        {  return const_iterator( fids, 4, 0 );  };
    };

  public:
    auto  GetForms() const -> const Forms&  {  return forms;  }
    auto  GetForms() -> Forms&  {  return forms;  }

  protected:
    Forms  forms;

  };

  template <class Allocator>
  class BaseImage
  {
    friend class Processor;

    using MarkupTag = DeliriX::MarkupTag;

    using MarkupTagAllocator = AllocatorCast<Allocator, MarkupTag>;
    using TextTokenAllocator = AllocatorCast<Allocator, TextToken>;
  public:
    BaseImage( Allocator = Allocator() );
    BaseImage( BaseImage&& );
    BaseImage& operator=( BaseImage&& );
   ~BaseImage();

  public:
    auto  AddBuffer( const widechar*, size_t ) -> const widechar*;

    auto  GetMarkup() const -> mtc::span<const MarkupTag>
      {  return markup;  }
    auto  GetTokens() const -> mtc::span<const TextToken>
      {  return tokens;  }
    auto  GetLemmas() const -> mtc::span<const mtc::span<const Lexeme>>
      {  return { (mtc::span<const Lexeme>*)lemmas.data(), lemmas.size() };  }

    auto  GetMarkup() -> std::vector<MarkupTag, MarkupTagAllocator>&  {  return markup;  }
    auto  GetTokens() -> std::vector<TextToken, TextTokenAllocator>&  {  return tokens;  }

    auto  Serialize( DeliriX::IText* ) -> DeliriX::IText*;

    void  clear();

  protected:
    void  clear_buf();

  protected:
    template <class C>
    using rebind = AllocatorCast<Allocator, C>;
    template <class C>
    using vector = std::vector<C, rebind<C>>;

    vector<widechar*>               bufbox;
    vector<MarkupTag>               markup;
    vector<TextToken>               tokens;
    vector<mtc::span<const Lexeme>> lemmas;
    vector<Lexeme>                  lexbuf;

  };

  using Image = BaseImage<std::allocator<char>>;

  // BaseImage template implementation

  template <class Allocator>
  BaseImage<Allocator>::BaseImage( Allocator a ):
    bufbox( a ),
    markup( a ),
    tokens( a ),
    lemmas( a ),
    lexbuf( a )
  {}

  template <class Allocator>
  BaseImage<Allocator>::BaseImage( BaseImage&& b ):
    bufbox( std::move( b.bufbox ) ),
    markup( std::move( b.markup ) ),
    tokens( std::move( b.tokens ) ),
    lemmas( std::move( b.lemmas ) ),
    lexbuf( std::move( b.lexbuf ) )
  {}

  template <class Allocator>
  auto  BaseImage<Allocator>::operator=( BaseImage&& b ) -> BaseImage&
  {
    bufbox = std::move( b.bufbox );
    markup = std::move( b.markup );
    tokens = std::move( b.tokens );
    lemmas = std::move( b.lemmas );
    lexbuf = std::move( b.lexbuf );
    return *this;
  }

  template <class Allocator>
  BaseImage<Allocator>::~BaseImage()
  {
    clear_buf();
  }

  template <class Allocator>
  auto  BaseImage<Allocator>::AddBuffer( const widechar* pws, size_t len ) -> const widechar*
  {
    auto  palloc = typename std::allocator_traits<Allocator>::template rebind_alloc<widechar>(
      bufbox.get_allocator() ).allocate( len );

    bufbox.push_back( (widechar*)memcpy(
      palloc, pws, len * sizeof(widechar) ) );

    return bufbox.back();
  }

  template <class Allocator>
  auto  BaseImage<Allocator>::Serialize( DeliriX::IText* text ) -> DeliriX::IText*
  {
    auto  lineIt = tokens.begin();
    auto  markIt = markup.begin();
    auto  fPrint = std::function<void( DeliriX::IText*, uint32_t )>();

    fPrint = [&]( DeliriX::IText* to, uint32_t up )
      {
        while ( lineIt != tokens.end() && lineIt - tokens.begin() <= up )
        {
        // check if print next line to current IText*
          if ( markIt == markup.end() || (lineIt - tokens.begin()) < markIt->uLower )
          {
            to->AddBlock( lineIt->pwsstr, lineIt->length );

            if ( ++lineIt == tokens.end() ) return;
              continue;
          }

        // check if open new span
          if ( lineIt - tokens.begin() >= markIt->uLower )
          {
            auto  new_to = to->AddMarkupTag( markIt->tagKey );
            auto  uUpper = markIt->uUpper;
              ++markIt;
            fPrint( new_to.ptr(), uUpper );
          }
        }
      };

    fPrint( text, -1 );

    return text;
  }

  template <class Allocator>
  void BaseImage<Allocator>::clear()
  {
    tokens.clear();
    markup.clear();
    lemmas.clear();
    clear_buf();
  }

  template <class Allocator>
  void BaseImage<Allocator>::clear_buf()
  {
    auto  deallocator = typename std::allocator_traits<Allocator>::template rebind_alloc<widechar>(
      bufbox.get_allocator() );

    for ( auto next: bufbox )
      deallocator.deallocate( next, 0 );

    bufbox.clear();
  }

}}

# endif   // !__DelphiX_context_text_image_hpp__
