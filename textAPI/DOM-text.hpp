# if !defined( __DelphiX_textAPI_DOM_text_hpp__ )
# define __DelphiX_textAPI_DOM_text_hpp__
# include "../text-api.hpp"
# include <moonycode/codes.h>
# include <mtc/serialize.h>
# include <mtc/wcsstr.h>
# include <functional>
# include <stdexcept>
# include <vector>
# include <string>

namespace DelphiX {
namespace textAPI {

  template <class Allocator>
  class BaseDocument final: public IText, public ITextView
  {
    class  Str;

    template <class To>
    using rebind = typename std::allocator_traits<Allocator>::template rebind_alloc<To>;

    template <class C>
    using basestr = std::basic_string<C, std::char_traits<C>, rebind<C>>;
    using charstr = basestr<char>;
    using widestr = basestr<widechar>;

    class Markup;

    implement_lifetime_stub

  protected:
    struct Item
    {
      friend class BaseDocument;

      Item( const char* s ):
        str( s ),
        tag( nullptr ),
        arr( nullptr )  {}
      Item( const char* t, const std::initializer_list<Item>& l ):
        str( nullptr ),
        tag( t ),
        arr( &l ) {}

    protected:
      const char*                         str;
      const char*                         tag;
      const std::initializer_list<Item>*  arr;
    };

  public:
    auto  AddTextTag( const char*, size_t = -1 ) -> mtc::api<IText> override;
    void  AddCharStr( const char*, size_t, unsigned ) override;
    void  AddWideStr( const widechar*, size_t ) override;

  public:
    BaseDocument( Allocator allocator = Allocator() ):
      lines( allocator ),
      spans( allocator )  {}
    BaseDocument( BaseDocument&& );
    BaseDocument( const BaseDocument& ) = delete;
    BaseDocument( const std::initializer_list<Item>& init, Allocator mman = Allocator() ):
      BaseDocument( init, codepages::codepage_utf8, mman ) {}
    BaseDocument( const std::initializer_list<Item>&, unsigned cp, Allocator = Allocator() );
   ~BaseDocument();

    BaseDocument& operator=( BaseDocument&& );

  public:
    void  clear();

    auto  GetBlocks() const -> Slice<const TextChunk> override  {  return lines;  }
    auto  GetMarkup() const -> Slice<const MarkupTag> override  {  return spans;  }
    auto  GetLength() const -> uint32_t override                {  return chars;  }

  public:
    auto  AddFormat( const std::string&, uint32_t, uint32_t ) -> MarkupTag&;

    auto  GetBufLen() const -> size_t;
  template <class O>
    auto  Serialize( O* ) const -> O*;
  template <class S>
    auto  FetchFrom( S* ) -> S*;
    auto  Serialize( IText* ) const -> IText*;

  protected:
    std::vector<TextChunk, Allocator> lines;
    std::vector<MarkupTag, Allocator> spans;
    mtc::api<Markup>                  pspan;
    uint32_t                          chars = 0;

  };

  using Document = BaseDocument<std::allocator<char>>;

  template <class Allocator>
  class BaseDocument<Allocator>::Markup final: public IText
  {
    friend class BaseDocument<Allocator>;

    std::atomic_long  refCount = 0;

  public:
    long  Attach() override {  return ++refCount;  }
    long  Detach() override;

  protected:
    auto  AddTextTag( const char*, size_t ) -> mtc::api<IText> override;
    void  AddCharStr( const char*, size_t, unsigned ) override;
    void  AddWideStr( const widechar*, size_t ) override;

    auto  LastMarkup( Markup* ) -> Markup*;
    void  Close();

  public:
    Markup( BaseDocument*, const char* tag, size_t length );
    Markup( Markup*, const char* tag, size_t length );
   ~Markup();

  protected:
    mtc::api<BaseDocument>  docptr;
    mtc::api<Markup>        nested;
    size_t                  tagBeg;

  };

  // Document implementation

  template <class Allocator>
  BaseDocument<Allocator>::BaseDocument( BaseDocument&& from )
  {
    operator=( std::move( from ) );    // important!!!
  }

  template <class Allocator>
  BaseDocument<Allocator>::BaseDocument( const std::initializer_list<Item>& init, unsigned codepage, Allocator mman ):
    lines( mman ),
    spans( mman )
  {
    auto  fnFill = std::function<void( mtc::api<IText>, const std::initializer_list<Item>& )>();

    fnFill = [&]( mtc::api<IText> to, const std::initializer_list<Item>& it )
      {
        for ( auto& next: it )
          if ( next.str != nullptr )  to->AddCharStr( next.str, -1, codepage );
            else  fnFill( to->AddMarkup( next.tag ), *next.arr );
      };

    fnFill( this, init );
  }

  template <class Allocator>
  BaseDocument<Allocator>::~BaseDocument()
  {
    clear();
  }

  template <class Allocator>
  auto  BaseDocument<Allocator>::operator = ( BaseDocument&& from ) -> BaseDocument&
  {
    pspan = nullptr;  from.pspan = nullptr;

    clear();

    lines = std::move( from.lines );
    spans = std::move( from.spans );
    chars = std::move( from.chars );

    return from.chars = 0, *this;
  }

  template <class Allocator>
  auto  BaseDocument<Allocator>::AddTextTag( const char* tag, size_t len ) -> mtc::api<IText>
  {
    if ( len == size_t(-1) )
      for ( ++len; tag[len] != 0; ++len ) (void)NULL;

    if ( pspan != nullptr )
      pspan->Close();

    return (pspan = new( rebind<Markup>( spans.get_allocator() ).allocate( 1 ) )
      Markup( this, tag, len )).ptr();
  }

  template <class Allocator>
  void  BaseDocument<Allocator>::AddCharStr( const char* str, size_t len, unsigned codepage )
  {
    char* pstr;

    if ( len == size_t(-1) )
      for ( ++len; str[len] != 0; ++len ) (void)NULL;

    if ( pspan != nullptr )
      pspan->Close();

    mtc::w_strncpy( pstr = rebind<char>( lines.get_allocator() ).allocate( len + 1 ),
      str, len )[len] = 0;

    lines.push_back( { (const void*)pstr, len, codepage } );
      chars += len;
  }

  template <class Allocator>
  void  BaseDocument<Allocator>::AddWideStr( const widechar* str, size_t len )
  {
    widechar* pstr;

    if ( len == size_t(-1) )
      for ( ++len; str[len] != 0; ++len ) (void)NULL;

    if ( pspan != nullptr )
      pspan->Close();

    mtc::w_strncpy( pstr = rebind<widechar>( lines.get_allocator() ).allocate( len + 1 ),
      str, len )[len] = 0;

    lines.push_back( { (const void*)pstr, len, unsigned(-1) } );
      chars += len;
  }

  template <class Allocator>
  void    BaseDocument<Allocator>::clear()
  {
    if ( pspan != nullptr )
      {  pspan->Close();  pspan = nullptr;  }

    for ( auto& next: lines )
    {
      if ( next.encode == unsigned(-1) )
        rebind<widechar>( lines.get_allocator() ).deallocate( (widechar*)next.strptr, 0 );
      else
        rebind<char>( lines.get_allocator() ).deallocate( (char*)next.strptr, 0 );
    }

    for ( auto& next: spans )
      rebind<char>( spans.get_allocator() ).deallocate( (char*)next.format, 0 );

    lines.clear();
    spans.clear();
    chars = 0;
  }

  template <class Allocator>
  auto  BaseDocument<Allocator>::AddFormat( const std::string& format, uint32_t ulower, uint32_t uupper ) -> MarkupTag&
  {
    auto  fmtstr = strcpy( rebind<char>( spans.get_allocator() ).allocate( format.length() + 1 ),
      format.c_str() );

    spans.push_back( { fmtstr, ulower, uupper } );
    return spans.back();
  }

  template <class Allocator>
  size_t  BaseDocument<Allocator>::GetBufLen() const
  {
    auto  length = ::GetBufLen( lines.size() ) + ::GetBufLen( spans.size() );

    for ( auto& str: lines )
    {
      length +=
        ::GetBufLen( str.encode )
      + ::GetBufLen( str.length ) + str.length * (str.encode == unsigned(-1) ? sizeof(widechar) : sizeof(char));
    }

    for ( auto& tag: spans )
    {
      length +=
        ::GetBufLen( tag.format )
      + ::GetBufLen( tag.uLower )
      + ::GetBufLen( tag.uUpper );
    }
    return length;
  }

  template <class Allocator>
  template <class O>
  O*  BaseDocument<Allocator>::Serialize( O* o ) const
  {
    o = ::Serialize( o, lines.size() );

    for ( auto& str: lines )
    {
      o = ::Serialize( ::Serialize( ::Serialize( o,
        str.encode ),
        str.length ),
        str.strptr, str.length * (str.encode == unsigned(-1) ? sizeof(widechar) : sizeof(char)) );
    }

    o = ::Serialize( o, spans.size() );

    for ( auto& tag: spans )
    {
      o = ::Serialize( ::Serialize( ::Serialize( o,
        tag.format ),
        tag.uLower ),
        tag.uUpper );
    }

    return o;
  }

  template <class Allocator>
  template <class S>
  S*  BaseDocument<Allocator>::FetchFrom( S* s )
  {
    size_t  length;

    clear();

  // get strings vector length value
    if ( (s = ::FetchFrom( s, length )) != nullptr )  lines.resize( length );
      else return nullptr;

  // get strings
    for ( auto& str: lines )
    {
      s = ::FetchFrom( ::FetchFrom( s,
        str.encode ),
        str.length );

      if ( str.encode == unsigned(-1) )
      {
        ((widechar*)(str.strptr = rebind<widechar>( lines.get_allocator() ).allocate( str.length + 1 )))
          [str.length] = 0;

        s = ::FetchFrom( s, (void*)str.strptr, sizeof(widechar) * str.length );
      }
        else
      {
        ((char*)(str.strptr = rebind<char>( lines.get_allocator() ).allocate( str.length + 1 )))
          [str.length] = 0;

        s = ::FetchFrom( s, (void*)str.strptr, str.length );
      }

      if ( s == nullptr ) return nullptr;
        else chars += length;
    }

    if ( (s = ::FetchFrom( s, length )) != nullptr )  spans.resize( length );
      else return nullptr;

    for ( auto& tag: spans )
    {
      size_t  taglen;

      if ( (s = ::FetchFrom( s, taglen )) == nullptr )
        return nullptr;

      ((char*)(tag.format = rebind<char>( spans.get_allocator() ).allocate( taglen + 1 )))
        [taglen] = 0;

      s = ::FetchFrom( ::FetchFrom( ::FetchFrom( s, (void*)tag.format, taglen ),
        tag.uLower ),
        tag.uUpper );

      if ( s == nullptr )
        return nullptr;
    }

    return s;
  }

  template <class Allocator>
  auto  BaseDocument<Allocator>::Serialize( IText* text ) const -> IText*
  {
    return textAPI::Serialize( text, *this );
  }

  // Document::Markup template implementation

  template <class Allocator>
  BaseDocument<Allocator>::Markup::Markup( BaseDocument* owner, const char* tag, size_t len ):
    docptr( owner ),
    tagBeg( owner->spans.size() )
  {
    auto  tagstr = strcpy( rebind<char>( owner->spans.get_allocator() )
      .allocate( len + 1 ), tag );

    owner->spans.push_back( { tagstr, uint32_t(owner->chars), uint32_t(-1) } );
  }

  template <class Allocator>
  BaseDocument<Allocator>::Markup::Markup( Markup* owner, const char* tag, size_t len ):
    docptr( owner->docptr ),
    tagBeg( docptr->spans.size() )
  {
    auto  tagstr = strcpy( rebind<char>( docptr->spans.get_allocator() )
      .allocate( len + 1 ), tag );

    docptr->spans.push_back( { tagstr, uint32_t(docptr->chars), uint32_t(-1) } );
  }

  template <class Allocator>
  BaseDocument<Allocator>::Markup::~Markup()
  {
    Close();
  }

  template <class Allocator>
  long  BaseDocument<Allocator>::Markup::Detach()
  {
    auto  rcount = --refCount;

    if ( rcount == 0 )
    {
      auto  memman = rebind<Markup>( docptr->lines.get_allocator() );

      this->~Markup();
      memman.deallocate( this, 0 );
    }
    return rcount;
  }

  template <class Allocator>
  auto  BaseDocument<Allocator>::Markup::AddTextTag( const char* tag, size_t len ) -> mtc::api<IText>
  {
    Markup* last;

    if ( len == size_t(-1) )
      for ( ++len; tag[len] != 0; ++len ) (void)NULL;

    if ( (last = LastMarkup( this )) != nullptr )
      last->Close();

    return (nested = new( rebind<Markup>( docptr->spans.get_allocator() ).allocate( 1 ) )
      Markup( this, tag, len )).ptr();
  }

  template <class Allocator>
  void  BaseDocument<Allocator>::Markup::AddCharStr( const char* str, size_t len, unsigned encoding )
  {
    Markup* last;
    char*   pstr;

    if ( tagBeg == size_t(-1) )
      throw std::logic_error( "attempt of adding line to closed markup" );

    if ( len == size_t(-1) )
      for ( ++len; str[len] != 0; ++len ) (void)NULL;

    if ( (last = LastMarkup( nullptr )) != this )
      last->Close();

    mtc::w_strncpy( pstr = rebind<char>( docptr->lines.get_allocator() ).allocate( len + 1 ),
      str, len )[len] = 0;

    docptr->lines.push_back( { pstr, len, encoding } );
      docptr->chars += len;
  }

  template <class Allocator>
  void  BaseDocument<Allocator>::Markup::AddWideStr( const widechar* str, size_t len )
  {
    Markup*   last;
    widechar* pstr;

    if ( tagBeg == size_t(-1) )
      throw std::logic_error( "attempt of adding line to closed markup" );

    if ( len == size_t(-1) )
      for ( ++len; str[len] != 0; ++len ) (void)NULL;

    if ( (last = LastMarkup( this )) != nullptr )
      last->Close();

    mtc::w_strncpy( pstr = rebind<widechar>( docptr->lines.get_allocator() ).allocate( len + 1 ),
      str, len )[len] = 0;

    docptr->lines.push_back( { pstr, len, unsigned(-1) } );
      docptr->chars += len;
  }

  template <class Allocator>
  auto  BaseDocument<Allocator>::Markup::LastMarkup( Markup* onPath ) -> Markup*
  {
    auto  markup = docptr->pspan;

    while ( markup != nullptr )
    {
      if ( markup == onPath )
        return nullptr;
      if ( markup->nested != nullptr )  markup = markup->nested;
        else return markup;
    }

    return markup;
  }

  template <class Allocator>
  void  BaseDocument<Allocator>::Markup::Close()
  {
    if ( nested != nullptr )
      {  nested->Close();  nested = nullptr;  }

    if ( tagBeg != size_t(-1) )
    {
      docptr->spans[tagBeg].uUpper = docptr->chars - 1;
        tagBeg = size_t(-1);
    }
  }

}}

# endif   // !__DelphiX_textAPI_DOM_text_hpp__
