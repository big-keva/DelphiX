# if !defined( __DelphiX_src_context_serial_formats_hpp__ )
# define __DelphiX_src_context_serial_formats_hpp__
# include "../../text-api.hpp"

namespace DelphiX {
namespace context {
namespace formats {

  class Compressor: protected std::vector<Compressor>, public textAPI::FormatTag<unsigned>
  {
    Compressor( const FormatTag& tag ): FormatTag( tag ) {}

  public:
    Compressor() = default;

    void  AddMarkup( const FormatTag& );
    void  SetMarkup( const Slice<FormatTag>& );
    auto  GetBufLen() const -> size_t;
    template <class O>
    auto  Serialize( O* ) const -> O*;
  };

  class Decompressor
  {
    using FormatTag = textAPI::FormatTag<unsigned>;

    struct Markup: FormatTag
    {
      int   nextCount;
      bool  hasNested;
    };

    const char* source;
    Markup      tatree[0x20];
    Markup*     loader = tatree - 1;

  public:
    Decompressor( const char* s ): source( s )
    {
      if ( (source = ::FetchFrom( s, tatree->nextCount )) != nullptr && tatree->nextCount-- != 0 )
        loader = LoadIt( *tatree ) ? tatree : nullptr;
    }

  public:
    auto  GetTag() const -> const FormatTag*
      {  return loader >= tatree ? loader : nullptr;  }
    auto  ToNext( unsigned = unsigned(-1) ) -> const FormatTag*;

  protected:
    bool  LoadIt( Markup& );

  };

  // Compressor implementation

  inline
  void  Compressor::AddMarkup( const FormatTag& tag )
  {
    if ( size() == 0 || tag.uLower > back().uUpper )
      return push_back( tag );
    if ( back().uLower <= tag.uLower && back().uUpper >= tag.uUpper )
      return back().AddMarkup( tag );
    throw std::logic_error( "invalid tags order" );
  }

  inline
  void  Compressor::SetMarkup( const Slice<FormatTag>& tags )
  {
    for ( auto& tag: tags )
      AddMarkup( tag );
  }

  inline
  size_t  Compressor::GetBufLen() const
  {
    auto  buflen = ::GetBufLen( size() );

    for ( auto& next: *this )
    {
      buflen += ::GetBufLen( next.format ) + ::GetBufLen( (next.uLower << 1) | (next.size() != 0 ? 1 : 0) )
        + ::GetBufLen( next.uUpper - next.uLower );

      if ( next.size() != 0 )
        buflen += next.GetBufLen();
    }

    return buflen;
  }

  template <class O>
  O*  Compressor::Serialize( O* o ) const
  {
    o = ::Serialize( o, this->size() );

    for ( auto& next: *this )
    {
      auto  uLower = (next.uLower << 1) | (next.size() != 0 ? 1 : 0);
      auto  uUpper = next.uUpper - next.uLower;

      o = ::Serialize( ::Serialize( ::Serialize( o, next.format ),
        uLower ),
        uUpper );

      if ( next.size() != 0 )
        o = next.Serialize( o );
    }

    return o;
  }

  // Decompressor implementation

  inline
  auto  Decompressor::ToNext( unsigned format ) -> const FormatTag*
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

}}}

# endif   // !__DelphiX_src_context_serial_formats_hpp__
