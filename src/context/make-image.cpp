# include "contents.hpp"
# include "lang-api.hpp"
# include "textAPI/word-break.hpp"
# include "context/index-keys.hpp"
# include "context/index-type.hpp"
# include <mtc/arbitrarymap.h>
# include <mtc/arena.hpp>
# include <memory>
#include <context/make-image.hpp>

namespace DelphiX {
namespace context {

  struct Elements
  {
    virtual ~Elements() = default;
    virtual auto  BlockType() const -> unsigned = 0;
    virtual auto  GetBufLen() const -> size_t = 0;
    virtual auto  Serialize( char* ) const -> char* = 0;
  };

  class Contents: public IImage
  {
  public:
    Contents():
      keyToPos( memArena.Create<ElementMap>() ) {}

  protected:
    static
    auto  CreateKey( unsigned idl, uint32_t cls, const widechar* str, size_t len ) -> Key
    {
      return cls != 0 ? Key( idl, cls, str, len ) : Key( idl, str, len );
    }

  public:
    template <class Compressor, class ... Args>
    void  AddRecord( const StrView&, const Args... );

    auto  get_allocator() -> mtc::Arena::allocator<char>  {  return memArena.get_allocator<char>();  }

  public:
    void  Enum( IContentsIndex::IIndexAPI* ) const override;

  protected:
    using ElementSet = Elements*;
    using ElementMap = mtc::arbitrarymap<ElementSet, mtc::Arena::allocator<char>>;

    mtc::Arena  memArena;
    ElementMap* keyToPos;

  };

  struct LongEntry
  {
    unsigned  pos;
    uint8_t   fid;

  public:
    LongEntry():
      pos( 0 ), fid( 0 ) {}
    LongEntry( unsigned p, uint8_t f ):
      pos( p ), fid( f ) {}

  public:
    auto  operator - ( const LongEntry& o ) const -> LongEntry
      {  return { pos - o.pos, fid };  }
    auto  operator - ( int n ) const -> LongEntry
      {  return { pos - n, fid };  }
    auto  operator -= ( const LongEntry& o ) -> LongEntry&
      {  return pos -= o.pos, *this;  }
    auto  operator -= ( int n ) -> LongEntry&
      {  return pos -= n, *this;  }

  public:
    auto    GetBufLen() const -> size_t {  return ::GetBufLen( pos ) + 1;  }
  template <class O>
    auto    Serialize( O* o ) const -> O* {  return ::Serialize( ::Serialize( o, pos ), fid );  }

  };

  using MiniEntry = unsigned;

  template <unsigned typeId, class Entry, class Alloc>
  class Compressor: public Elements, protected std::vector<Entry, Alloc>
  {
  public:
    enum: unsigned {  objectType = typeId  };

  public:
    Compressor( Alloc alloc ): std::vector<Entry, Alloc>( alloc )
    {
    }
    template <class ... Args>
    void  AddRecord( Args... args )
    {
      if ( this->size() == this->capacity() )
        this->reserve( this->size() + 0x10 );
      this->emplace_back( args... );
    }
    auto  BlockType() const -> unsigned override  {  return objectType;  }
    auto  GetBufLen() const -> size_t override
    {
      auto  ptrbeg = this->begin();
      auto  oldent = *ptrbeg++;
      auto  length = ::GetBufLen( oldent );

      for ( ; ptrbeg != this->end(); oldent = *ptrbeg++ )
        length += ::GetBufLen( *ptrbeg - oldent - 1 );

      return ::GetBufLen( length ) + length;
    }
    char* Serialize( char* o ) const override
    {
      if ( (o = ::Serialize( o, GetBufLen() )) != nullptr )
      {
        auto  ptrbeg = this->begin();
        auto  oldent = *ptrbeg++;

        for ( o = ::Serialize( o, oldent ); ptrbeg != this->end() && o != nullptr; oldent = *ptrbeg++ )
          o = ::Serialize( o, *ptrbeg - oldent - 1 );
      }

      return o;
    }
  };

  class EntryCounter: public Elements
  {
    unsigned  count = 0;

  public:
    enum: unsigned {  objectType = EntryBlockType::EntryCount };

  template <class ... Args>
      EntryCounter( Args... ) {}

  template <class ... Args>
    void  AddRecord( Args... ) {  ++count;  }
    auto  BlockType() const -> unsigned override  {  return objectType;  }
    auto  GetBufLen() const -> size_t override    {  return ::GetBufLen( count );  }
    char* Serialize( char* o ) const override     {  return ::Serialize( o, count );  }
  };

  class EntryIgnored: public Elements
  {
  public:
    enum: unsigned {  objectType = EntryBlockType::None };

  template <class ... Args>
    EntryIgnored( Args... ) {}

  template <class ... Args>
    void  AddRecord( Args... ) {}
    auto  BlockType() const -> unsigned override  {  return objectType;  }
    auto  GetBufLen() const -> size_t override    {  return 0;  }
    char* Serialize( char* o ) const override     {  return o;  }
  };

  // Contents template implementation

  template <class Compressor, class ... Args>
  void  Contents::AddRecord( const StrView& key, const Args ... args )
  {
    auto  pfound = keyToPos->Search( key.data(), key.size() );

    if ( pfound == nullptr )
    {
      auto  palloc = memArena.Create<Compressor>();
      pfound = keyToPos->Insert( key.data(), key.size(), palloc );
    }

    if ( (*pfound)->BlockType() != Compressor::objectType )
      throw std::logic_error( "block type mismatch" );

    return ((Compressor*)(*pfound))->AddRecord( args... );
  }

  void  Contents::Enum( IContentsIndex::IIndexAPI* index ) const
  {
    std::unique_ptr<std::vector<char>>  valbuf;
    char                                buffer[0x400];

    for ( auto next = keyToPos->Enum( nullptr ); next != nullptr; next = keyToPos->Enum( next ) )
    {
      const auto& key = ElementMap::GetKey( next );
      const auto  len = ElementMap::KeyLen( next );
      const auto& val = ElementMap::GetVal( next );
      auto        cch = val->GetBufLen();

      if ( cch <= sizeof( buffer ) )
      {
        val->Serialize( buffer );

        index->Insert( { (const char*)key, len }, { buffer, cch },
          val->BlockType() );
      }
        else
      {
        if ( valbuf == nullptr )
          valbuf = std::make_unique<std::vector<char>>( cch );

        if ( valbuf->size() < cch )
          valbuf->resize( cch );

        val->Serialize(
          valbuf->data() );

        index->Insert( { (const char*)key, len }, { valbuf->data(), cch },
          val->BlockType() );
      }
    }
  }

  class RichContents final: public Contents
  {
    implement_lifetime_control

  public:
    void  AddTerm(
      unsigned  pos,
      unsigned  idl,
      uint32_t  lex, const uint8_t* fms, size_t cnt ) override
    {
      auto  key = Key( idl, lex );

      if ( cnt > 1 && *fms != 0xff )
        return AddRecord<Compressor<EntryBlockType::FormsOrder, LongEntry, mtc::Arena::allocator<char>>>( StrView( key ), unsigned(pos), *fms );
      else
        return AddRecord<Compressor<EntryBlockType::EntryOrder, MiniEntry, mtc::Arena::allocator<char>>>( key, pos );
    }
    void  AddStem(
      unsigned        pos,
      unsigned        idl,
      const widechar* str,
      size_t          len,
      uint32_t        cls, const uint8_t* fms, size_t cnt ) override
    {
      auto  key = CreateKey( idl, cls, str, len );

      if ( cnt > 1 && *fms != 0xff )
        return AddRecord<Compressor<EntryBlockType::FormsOrder, LongEntry, mtc::Arena::allocator<char>>>( StrView( key ), unsigned(pos), *fms );
      else
        return AddRecord<Compressor<EntryBlockType::EntryOrder, MiniEntry, mtc::Arena::allocator<char>>>( key, pos );
    }
  };

  class BM25Contents final: public Contents
  {
    implement_lifetime_control

  public:
    void  AddTerm(
      unsigned  pos,
      unsigned  idl,
      uint32_t  lex, const uint8_t* fms, size_t cnt ) override
    {
      (void)pos, (void)fms, (void)cnt;

      return AddRecord<EntryCounter>( Key( idl, lex ) );
    }
    void  AddStem(
      unsigned        pos,
      unsigned        idl,
      const widechar* str,
      size_t          len,
      uint32_t        cls, const uint8_t* fms, size_t cnt ) override
    {
      (void)pos, (void)fms, (void)cnt;

      return AddRecord<EntryCounter>( CreateKey( idl, cls, str, len ) );
    }
  };

  class LiteContents final: public Contents
  {
    implement_lifetime_control

  public:
    void  AddTerm(
      unsigned  pos,
      unsigned  idl,
      uint32_t  lex, const uint8_t* fms, size_t cnt ) override
    {
      (void)pos, (void)fms, (void)cnt;

      return AddRecord<EntryIgnored>( Key( idl, lex ) );
    }
    void  AddStem(
      unsigned        pos,
      unsigned        idl,
      const widechar* str,
      size_t          len,
      uint32_t        cls, const uint8_t* fms, size_t cnt ) override
    {
      (void)pos, (void)fms, (void)cnt;

      return AddRecord<EntryIgnored>( CreateKey( idl, cls, str, len ) );
    }
  };

  auto  Lite::Create() -> mtc::api<IImage>
  {
    return mtc::api<IImage>( new LiteContents() );
  }

  auto  BM25::Create() -> mtc::api<IImage>
  {
    return mtc::api<IImage>( new BM25Contents() );
  }

  auto  Rich::Create() -> mtc::api<IImage>
  {
    return mtc::api<IImage>( new RichContents() );
  }

}}
