# if !defined( __DelphiX_src_context_formats_hpp__ )
# define __DelphiX_src_context_formats_hpp__
# include "../../text-api.hpp"

namespace DelphiX {
namespace context {
namespace formats {

  template <class Allocator = std::allocator<char>>
  class Compressor: protected std::vector<Compressor<Allocator>, Allocator>, public textAPI::FormatTag<unsigned>
  {
  public:
    Compressor( Allocator mem = Allocator() ):
      std::vector<Compressor, Allocator>( mem ), FormatTag{ 0, 0, 0 } {}
    Compressor( const FormatTag& tag, Allocator mem = Allocator() ):
      std::vector<Compressor, Allocator>( mem ), FormatTag( tag )  {}

    using entry_type = FormatTag;

  public:
    void  AddMarkup( const FormatTag& );
    void  SetMarkup( const Slice<FormatTag>& );
    auto  GetBufLen() const -> size_t;
    template <class O>
    auto  Serialize( O* ) const -> O*;
  };

  inline  auto  Unpack(
    textAPI::FormatTag<unsigned>* tbeg,
    textAPI::FormatTag<unsigned>* tend,
    const char*&                  pbeg,
    const char*                   pend,
    unsigned                      base = 0 ) -> unsigned
  {
    auto  torg( tbeg );
    int   size;

    if ( (pbeg = ::FetchFrom( pbeg, size )) == nullptr || pbeg == pend )
      return 0;

    while ( size-- > 0 )
    {
      unsigned  format;
      unsigned  ulower;
      unsigned  uupper;

      if ( (pbeg = ::FetchFrom( ::FetchFrom( ::FetchFrom( pbeg,
        format ),
        ulower ),
        uupper )) == nullptr )
      break;

      *tbeg++ = { format >> 1, ulower + base, ulower + base + uupper };

      if ( format & 1 )
        tbeg += Unpack( tbeg, tend, pbeg, pend, ulower + base );
    }
    return tbeg - torg;
  }

  inline  auto  Unpack(
    textAPI::FormatTag<unsigned>* tbeg, size_t tlen,
    const char*                   pbeg, size_t size ) -> unsigned
  {
    return Unpack( tbeg, tbeg + tlen, pbeg, pbeg + size );
  }

  template <size_t N>
  auto  Unpack(
    textAPI::FormatTag<unsigned>(&tags)[N],
    const char*                   pbeg,
    const char*                   pend ) -> unsigned
  {
    return Unpack( tags, tags + N, pbeg, pend );
  }

  template <size_t N>
  auto  Unpack(
    textAPI::FormatTag<unsigned>(&tags)[N], const char* pbeg, size_t size ) -> unsigned
  {
    return Unpack( tags, tags + N, pbeg, pbeg + size );
  }

  // Compressor implementation

  template <class Allocator>
  void  Compressor<Allocator>::AddMarkup( const FormatTag& tag )
  {
    if ( this->empty() || tag.uLower > this->back().uUpper )
      return (void)this->emplace_back( tag, this->get_allocator() );
    if ( this->back().uLower <= tag.uLower && this->back().uUpper >= tag.uUpper )
      return (void)this->back().AddMarkup( tag );
    throw std::logic_error( "invalid tags order" );
  }

  template <class Allocator>
  void  Compressor<Allocator>::SetMarkup( const Slice<FormatTag>& tags )
  {
    for ( auto& tag: tags )
      AddMarkup( tag );
  }

  template <class Allocator>
  size_t  Compressor<Allocator>::GetBufLen() const
  {
    auto  buflen = ::GetBufLen( this->size() );

    for ( auto& next: *this )
    {
      auto  loDiff = next.uLower - uLower;
      auto  upDiff = next.uUpper - next.uLower;
      auto  fStore = (next.format << 1) | (next.size() != 0 ? 1 : 0);

      buflen +=
        ::GetBufLen( fStore ) +
        ::GetBufLen( loDiff ) +
        ::GetBufLen( upDiff );

      if ( next.size() != 0 )
        buflen += next.GetBufLen();
    }

    return buflen;
  }

  template <class Allocator>
  template <class O>
  O*  Compressor<Allocator>::Serialize( O* o ) const
  {
    o = ::Serialize( o, this->size() );

    for ( auto& next: *this )
    {
      auto  loDiff = next.uLower - uLower;
      auto  upDiff = next.uUpper - next.uLower;
      auto  fStore = (next.format << 1) | (next.size() != 0 ? 1 : 0);

      o = ::Serialize( ::Serialize( ::Serialize( o,
        fStore ),
        loDiff ),
        upDiff );

      if ( next.size() != 0 )
        o = next.Serialize( o );
    }

    return o;
  }

  // Decompressor implementation
/*
  inline
  auto  Decompressor::GetNext( unsigned format ) -> const FormatTag*
  {
    for ( auto p = loader; p >= tatree; )
    {
      // если есть вложенные элементы, перейти к первому из них
      if ( p->hasNested )
      {
        if ( (source = ::FetchFrom( source, p[1].nextCount )) == nullptr )
          return loader = nullptr;
        (p++)->hasNested = false;
      }

      // вложенных элементов нет, однако могут быть элементы в цепочке;
      // загрузить следующий
      if ( p->nextCount-- == 0 )
      {  --p;  continue;  }

      if ( !LoadIt( *p ) )
        return loader = nullptr;

      if ( format == unsigned(-1) || format == p->format )
        return loader = p;
    }
    return loader = nullptr;
  }

  inline
  auto  Decompressor::FindTag( unsigned offset ) -> const FormatTag*
  {
    const FormatTag* select = nullptr;

  // check the trace for covering element
    for ( auto p = loader; p >= tatree && select == nullptr; --p )
      if ( p->uLower <= offset && p->uUpper >= offset )
        select = p;

  // now select is potentially best element; try lookup the format trace
    for ( auto p = loader; p >= tatree && p->uLower <= offset; )
    {
      // если есть вложенные элементы, перейти к первому из них
      if ( p->hasNested )
      {
        if ( (source = ::FetchFrom( source, (loader = p + 1)->nextCount )) == nullptr )
          return loader = nullptr, select;
        (p++)->hasNested = false;
          continue;
      }

      // вложенных элементов нет, однако могут быть элементы в цепочке;
      // загрузить следующий
      if ( p->nextCount-- == 0 )
        {  --p;  continue;  }

      if ( !LoadIt( *(loader = p) ) )
        return loader = nullptr, select;

      if ( p->uLower > offset )
        break;
      if ( p->uUpper >= offset )
        select = p;
    }
    return select;
  }

  inline
  bool  Decompressor::LoadIt( Markup& tag )
  {
    if ( (source = ::FetchFrom( ::FetchFrom( ::FetchFrom( source,
      tag.format ),
      tag.uLower ),
      tag.uUpper )) != nullptr )
    {
      tag.hasNested = (tag.uLower & 0x01) != 0;
      tag.uLower >>= 1;
      tag.uUpper += tag.uLower;
    }
    return source != nullptr;
  }
*/

}}}

# endif   // !__DelphiX_src_context_formats_hpp__
