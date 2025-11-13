# include "../../context/pack-format.hpp"
# include "../../contents.hpp"
# include "../../compat.hpp"

using SerialFn = std::function<void(const void*, size_t)>;

template <> inline
SerialFn* Serialize( SerialFn* f, const void* p, size_t l )
  {  return f != nullptr ? (*f)( p, l ), f : nullptr;  }

template <> inline
std::vector<char>* Serialize( std::vector<char>* o, const void* p, size_t l )
  {  return o != nullptr ? o->insert( o->end(), (const char*)p, l + (const char*)p ), o : nullptr;  }

namespace DelphiX {
namespace context {
namespace formats {

  template <class Allocator = std::allocator<char>>
  class Compressor: protected std::vector<Compressor<Allocator>, AllocatorCast<Allocator, Compressor<Allocator>>>,
    public RankerTag
  {
  public:
    Compressor( Allocator mem = Allocator() ): std::vector<Compressor, AllocatorCast<Allocator, Compressor>>( mem ),
      RankerTag{ 0, 0, 0 } {}
    Compressor( const RankerTag& tag, Allocator mem = Allocator() ): std::vector<Compressor, AllocatorCast<Allocator, Compressor>>( mem ),
      RankerTag( tag )  {}

    using entry_type = RankerTag;

  public:
    void  AddMarkup( const RankerTag& );
    auto  SetMarkup( const mtc::span<const RankerTag>& ) -> Compressor&;
    auto  GetBufLen() const -> size_t;
    template <class O>
    auto  Serialize( O* ) const -> O*;
  };

  // Pack formats funcs

  template <class O>
  void  Pack( O* o, const mtc::span<const RankerTag>& in )
  {
    Compressor().SetMarkup( in ).Serialize( o );
  }

  template <class O>
  void  Pack( O* o, const mtc::span<const MarkupTag>& in, FieldHandler& fh )
  {
    Compressor  compressor;

    for ( auto& ft: in )
    {
      auto  pf = fh.Add( ft.tagKey );

      if ( pf != nullptr )
        compressor.AddMarkup( { pf->id, ft.uLower, ft.uUpper } );
    }
    compressor.Serialize( o );
  }

  void  Pack( std::function<void(const void*, size_t)> fn, const mtc::span<const RankerTag>& in )
    {  return Pack<SerialFn>( &fn, in );  }
  void  Pack( std::function<void(const void*, size_t)> fn, const mtc::span<const MarkupTag>& in, FieldHandler& fh )
    {  return Pack<SerialFn>( &fn, in, fh );  }
  void  Pack( mtc::IByteStream* ps, const mtc::span<const RankerTag>& in )
    {  return Pack<mtc::IByteStream>( ps, in );  }
  void  Pack( mtc::IByteStream* ps, const mtc::span<const DeliriX::MarkupTag>& in, FieldHandler& fh )
    {  return Pack<mtc::IByteStream>( ps, in, fh );  }

  auto  Pack( const mtc::span<const RankerTag>& in ) -> std::vector<char>
  {
    std::vector<char> out;

    return Pack( &out, in ), out;
  }

  auto  Pack( const mtc::span<const DeliriX::MarkupTag>& in, FieldHandler& fh ) -> std::vector<char>
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
    RankerTag*    tbeg,
    RankerTag*    tend,
    const char*&  pbeg,
    const char*   pend,
    unsigned      base = 0 ) -> size_t
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
    RankerTag*  tbeg, RankerTag*  tend,
    const char* pbeg, const char* pend ) -> size_t
  {
    return Unpack( tbeg, tend, pbeg, pend, 0 );
  }

  auto  Unpack( const mtc::span<const char>& pack ) -> std::vector<RankerTag>
  {
    auto  vout = std::vector<RankerTag>();
    auto  sptr = pack.data();

    Unpack( [&]( const RankerTag& tag )
      {  vout.push_back( tag );  }, sptr, pack.end(), 0 );

    return vout;
  }

  void  Unpack( std::function<void( const RankerTag& )> fAdd, const char* pbeg, const char* pend )
  {
    return Unpack( fAdd, pbeg, pend, 0 );
  }

  // Compressor implementation

  template <class Allocator>
  void  Compressor<Allocator>::AddMarkup( const RankerTag& tag )
  {
    if ( this->empty() || tag.uLower > this->back().uUpper )
      return (void)this->emplace_back( tag, this->get_allocator() );
    if ( this->back().uLower <= tag.uLower && this->back().uUpper >= tag.uUpper )
      return (void)this->back().AddMarkup( tag );
    throw std::logic_error( "invalid tags order @" __FILE__ ":" LINE_STRING );
  }

  template <class Allocator>
  auto  Compressor<Allocator>::SetMarkup( const mtc::span<const RankerTag>& tags ) -> Compressor&
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

}}}
