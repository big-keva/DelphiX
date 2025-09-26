# include "../../context/formats.hpp"
# include "../../contents.hpp"

using SerialFn = std::function<void(const void*, size_t)>;

template <>
SerialFn* Serialize( SerialFn* f, const void* p, size_t l )
{
  return f != nullptr ? (*f)( p, l ), f : nullptr;
}

template <>
std::vector<char>* Serialize( std::vector<char>* o, const void* p, size_t l )
{
  return o != nullptr ? o->insert( o->end(), (const char*)p, l + (const char*)p ), o : nullptr;
}

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
    auto  SetMarkup( const Slice<const FormatTag>& ) -> Compressor&;
    auto  GetBufLen() const -> size_t;
    template <class O>
    auto  Serialize( O* ) const -> O*;
  };

  // Pack formats funcs

  template <class O>
  void  Pack( O* o, const Slice<const FormatTag<unsigned>>& in )
  {
    Compressor().SetMarkup( in ).Serialize( o );
  }

  template <class O>
  void  Pack( O* o, const Slice<const FormatTag<const char*>>& in, FieldHandler& fh )
  {
    Compressor  compressor;

    for ( auto& ft: in )
    {
      auto  pf = fh.Add( ft.format );

      if ( pf != nullptr )
        compressor.AddMarkup( { pf->id, ft.uLower, ft.uUpper } );
    }
    compressor.Serialize( o );
  }

  void  Pack( std::function<void(const void*, size_t)> fn, const Slice<const FormatTag<unsigned>>& in )
    {  return Pack<SerialFn>( &fn, in );  }
  void  Pack( std::function<void(const void*, size_t)> fn, const Slice<const FormatTag<const char*>>& in, FieldHandler& fh )
    {  return Pack<SerialFn>( &fn, in, fh );  }
  void  Pack( mtc::IByteStream* ps, const Slice<const FormatTag<unsigned>>& in )
    {  return Pack<mtc::IByteStream>( ps, in );  }
  void  Pack( mtc::IByteStream* ps, const Slice<const FormatTag<const char*>>& in, FieldHandler& fh )
    {  return Pack<mtc::IByteStream>( ps, in, fh );  }

  auto  Pack( const Slice<const FormatTag<unsigned>>& in ) -> std::vector<char>
  {
    std::vector<char> out;

    return Pack( &out, in ), out;
  }

  auto  Pack( const Slice<const FormatTag<const char*>>& in, FieldHandler& fh ) -> std::vector<char>
  {
    std::vector<char> out;

    return Pack( &out, in, fh ), out;
  }


  // Unpack formats family

  template <class FnFormat>
  void  Unpack( FnFormat  fAdd, const char*& pbeg, const char* pend, unsigned base = 0 )
  {
    int   size;

    if ( (pbeg = ::FetchFrom( pbeg, size )) == nullptr || pbeg == pend )
      return;

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

      fAdd( { format >> 1, ulower + base, ulower + base + uupper } );

      if ( format & 1 )
        Unpack( fAdd, pbeg, pend, ulower + base );
    }
  }

  inline
  auto  Unpack(
    textAPI::FormatTag<unsigned>* tbeg,
    textAPI::FormatTag<unsigned>* tend,
    const char*&                  pbeg,
    const char*                   pend,
    unsigned                      base = 0 ) -> size_t
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

  auto  Unpack(
    FormatTag<unsigned>*  tbeg, FormatTag<unsigned>*  tend,
    const char*           pbeg, const char*           pend ) -> size_t
  {
    return Unpack( tbeg, tend, pbeg, pend, 0 );
  }

  auto  Unpack( const Slice<const char>& pack ) -> std::vector<FormatTag<unsigned>>
  {
    auto  vout = std::vector<FormatTag<unsigned>>();
    auto  sptr = pack.data();

    Unpack( [&]( const FormatTag<unsigned>& tag )
      {  vout.push_back( tag );  }, sptr, pack.end(), 0 );

    return vout;
  }

  void  Unpack( std::function<void( const FormatTag<unsigned>& )> fAdd, const char* pbeg, const char* pend )
  {
    return Unpack( fAdd, pbeg, pend, 0 );
  }

  // Compressor implementation

  template <class Allocator>
  void  Compressor<Allocator>::AddMarkup( const FormatTag& tag )
  {
    if ( this->empty() || tag.uLower > this->back().uUpper )
      return (void)this->emplace_back( tag, this->get_allocator() );
    if ( this->back().uLower <= tag.uLower && this->back().uUpper >= tag.uUpper )
      return (void)this->back().AddMarkup( tag );
    throw std::logic_error( "invalid tags order @" __FILE__ ":" LINE_STRING );
  }

  template <class Allocator>
  auto  Compressor<Allocator>::SetMarkup( const Slice<const FormatTag>& tags ) -> Compressor&
  {
    for ( auto& tag: tags )
      AddMarkup( tag );
    return *this;
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
