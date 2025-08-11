# include "../../context/x-contents.hpp"
# include "serial-formats.hpp"
# include <mtc/arbitrarymap.h>
# include <mtc/arena.hpp>

namespace DelphiX {
namespace context {

  struct RichEntry
  {
    unsigned  pos = 0;
    uint8_t   fid = 0;

  public:
    RichEntry() = default;
    RichEntry( unsigned p, uint8_t f ): pos( p ), fid( f ) {}

  public:
    auto  operator - ( const RichEntry& o ) const -> RichEntry  {  return { pos - o.pos, fid };  }
    auto  operator - ( int n ) const -> RichEntry          {  return { pos - n, fid };  }
    auto  operator -= ( const RichEntry& o ) -> RichEntry&      {  return pos -= o.pos, *this;  }
    auto  operator -= ( int n ) -> RichEntry&              {  return pos -= n, *this;  }

  public:
    auto    GetBufLen() const -> size_t {  return ::GetBufLen( pos ) + 1;  }
  template <class O>
    auto    Serialize( O* o ) const -> O* {  return ::Serialize( ::Serialize( o, pos ), fid );  }

  };

  class Contents final: public IContents
  {
    implement_lifetime_control

  public:
    struct Entries
    {
      virtual      ~Entries() = default;
      virtual auto  BlockType() const -> unsigned = 0;
      virtual auto  GetBufLen() const -> size_t = 0;
      virtual auto  Serialize( char* ) const -> char* = 0;
    };

    using AllocatorType = mtc::Arena::allocator<char>;

  public:
    Contents():
      keyToPos( memArena.Create<EntriesMap>() ) {}

    template <class Compressor>
    void  AddEntry( const Key&, const typename Compressor::entry_type& );
    void  SetBlock( const Key&, unsigned typeId, const Slice<const char>& );

  public:      // overridables from IContents
    void  Enum( IContentsIndex::IIndexAPI* ) const override;

  protected:
    using EntriesMap = mtc::arbitrarymap<Entries*, mtc::Arena::allocator<char>>;

    mtc::Arena  memArena;
    EntriesMap* keyToPos;

  };

  template <unsigned typeId>
  class ReferedKey: public Contents::Entries
  {
  public:
    enum: unsigned {  objectType = typeId };

    using entry_type = struct{};

  template <class Allocator>
    ReferedKey( Allocator ) {}
  template <class ... Args>
    void  AddRecord( Args... ) {}
    auto  BlockType() const -> unsigned override  {  return objectType;  }
    auto  GetBufLen() const -> size_t override    {  return 0;  }
    char* Serialize( char* o ) const override     {  return o;  }
  };

  template <unsigned typeId>
  class CountWords: public Contents::Entries
  {
    unsigned  count = 0;

  public:
    enum: unsigned {  objectType = typeId };

    using entry_type = struct{};

  template <class Allocator>
    CountWords( Allocator ) {}
  template <class ... Args>
    void  AddRecord( Args... ) {  ++count;  }
    auto  BlockType() const -> unsigned override  {  return objectType;  }
    auto  GetBufLen() const -> size_t override    {  return ::GetBufLen( count );  }
    char* Serialize( char* o ) const override     {  return ::Serialize( o, count );  }
  };

  template <unsigned typeId, class Entry, class Alloc>
  class Compressor: public Contents::Entries, protected std::vector<Entry, Alloc>
  {
  public:
    enum: unsigned {  objectType = typeId  };

    using entry_type = Entry;

  public:
    Compressor( Alloc alloc ): std::vector<Entry, Alloc>( alloc ) {}

    void  AddRecord( const entry_type& entry )
    {
      if ( this->size() == this->capacity() )
        this->reserve( this->size() + 0x10 );
      this->push_back( entry );
    }
    auto  BlockType() const -> unsigned override
    {
      return objectType;
    }
    auto  GetBufLen() const -> size_t override
    {
      auto  ptrbeg = this->begin();
      auto  oldent = *ptrbeg++;
      auto  length = ::GetBufLen( oldent );

      for ( ; ptrbeg != this->end(); oldent = *ptrbeg++ )
        length += ::GetBufLen( *ptrbeg - oldent - 1 );

      return length;
    }
    char* Serialize( char* o ) const override
    {
      auto  ptrbeg = this->begin();
      auto  oldent = *ptrbeg++;

      for ( o = ::Serialize( o, oldent ); ptrbeg != this->end() && o != nullptr; oldent = *ptrbeg++ )
        o = ::Serialize( o, *ptrbeg - oldent - 1 );

      return o;
    }

  };

  template <unsigned typeId, class Allocator>
  class TagCompressor: public Contents::Entries, public formats::Compressor<Allocator>
  {
    using formats::Compressor<Allocator>::Compressor;

  public:
    enum: unsigned {  objectType = typeId  };

    using entry_type = textAPI::FormatTag<unsigned>;

  public:
    void  AddRecord( const entry_type& entry )          {  return formats::Compressor<Allocator>::AddMarkup( entry );  }
    auto  BlockType() const -> unsigned override        {  return objectType;  }
    auto  GetBufLen() const -> size_t override          {  return formats::Compressor<Allocator>::GetBufLen();  }
    auto  Serialize( char* o ) const -> char* override  {  return formats::Compressor<Allocator>::Serialize( o );  }

  };

  // Contents implementation

  template <class Compressor>
  void  Contents::AddEntry( const Key& key, const typename Compressor::entry_type& ent )
  {
    auto  pblock = keyToPos->Search( key.data(), key.size() );

    if ( pblock == nullptr )
    {
      pblock = keyToPos->Insert( key.data(), key.size(),
        memArena.Create<Compressor>() );
    }

    if ( (*pblock)->BlockType() != Compressor::objectType )
      throw std::invalid_argument( "object types differ for different entries" );

    return ((Compressor*)(*pblock))->AddRecord( ent );
  }

  void  Contents::Enum( IContentsIndex::IIndexAPI* index ) const
  {
    char              stabuf[0x100];
    std::vector<char> dynbuf;

    if ( index == nullptr )
      throw std::invalid_argument( "invalid (null) call parameter" );

    for ( auto next = keyToPos->Enum( nullptr ); next != nullptr; next = keyToPos->Enum( next ) )
    {
      auto    keyptr = keyToPos->GetKey( next );
      auto    keylen = keyToPos->KeyLen( next );
      auto    pvalue = keyToPos->GetVal( next );
      size_t  vallen = pvalue->GetBufLen();
      char*   endptr;

      if ( vallen <= sizeof(stabuf) )
      {
        endptr = pvalue->Serialize( stabuf );

        if ( endptr != stabuf + vallen )
          throw std::logic_error( "entries serialization fault" );

        index->Insert( { (const char*)keyptr, keylen }, { stabuf, size_t(endptr - stabuf) },
          pvalue->BlockType() );
      }
        else
      {
        if ( vallen > dynbuf.size() )
          dynbuf.resize( (vallen + 0x100 - 1) & ~(0x100 - 1) );

        endptr = pvalue->Serialize( dynbuf.data() );

        if ( endptr != dynbuf.data() + vallen )
          throw std::logic_error( "entries serialization fault" );

        index->Insert( { (const char*)keyptr, keylen }, { dynbuf.data(), size_t(endptr - dynbuf.data()) },
          pvalue->BlockType() );
      }
    }
  }

  // Context creation functions

  auto  GetMiniContents(
    const Slice<const Slice<const Lexeme>>& lemm,
    const Slice<const textAPI::MarkupTag>&  mkup, FieldHandler& ) -> mtc::api<IContents>
  {
    auto  contents = mtc::api<Contents>( new Contents() );

    for ( auto& word: lemm )
      for ( auto& term: word )
        contents->AddEntry<ReferedKey<0>>( term, {} );

    return (void)mkup, contents.ptr();
  }

  auto  GetBM25Contents(
    const Slice<const Slice<const Lexeme>>& lemm,
    const Slice<const textAPI::MarkupTag>&  mkup, FieldHandler& ) -> mtc::api<IContents>
  {
    auto  contents = mtc::api<Contents>( new Contents() );

    for ( auto& word: lemm )
      for ( auto& term: word )
        contents->AddEntry<CountWords<10>>( term, {} );

    return (void)mkup, contents.ptr();
  }

  auto  GetRichContents(
    const Slice<const Slice<const Lexeme>>& lemm,
    const Slice<const textAPI::MarkupTag>&  mkup,
    FieldHandler&                           fman ) -> mtc::api<IContents>
  {
    auto  contents = mtc::api( new Contents() );

    for ( unsigned i = 0; i != lemm.size(); ++i )
      for ( auto& term: lemm[i] )
      {
        if ( term.GetForms().empty() || term.GetForms().front() == 0xff )
          contents->AddEntry<Compressor<20, unsigned, Contents::AllocatorType>>( term, i );
        else
          contents->AddEntry<Compressor<21, RichEntry, Contents::AllocatorType>>( term, { i, term.GetForms().front() } );
      }

    for ( auto& next: mkup )
    {
      auto  pfield = fman.Get( next.format );
      auto  fmtkey = Key( "format" );

      if ( pfield != nullptr )
        contents->AddEntry<TagCompressor<99, Contents::AllocatorType>>( fmtkey, { pfield->id, next.uLower, next.uUpper } );
    }

    return (void)mkup, contents.ptr();
  }

}}
