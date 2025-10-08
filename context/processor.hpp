# if !defined( __DelphiX_context_processor_hpp__ )
# define __DelphiX_context_processor_hpp__
# include "../lang-api.hpp"
# include "../textAPI/DOM-text.hpp"
# include "text-image.hpp"
# include "fields-man.hpp"
# include <moonycode/chartype.h>
# include <mtc/arbitrarymap.h>

namespace DelphiX {
namespace context {

  class Processor
  {
    struct Lemmatizer
    {
      unsigned               langId;
      mtc::api<ILemmatizer>  module;
    };

    template <class Allocator>  class InsertTerms;

  public:
    enum: size_t
    {
      max_word_length = 64
    };

  template <class Allocator>
    auto  Lemmatize( BaseImage<Allocator>& ) const -> BaseImage<Allocator>&;
  template <class Allocator>
    auto  Lemmatize( std::vector<Lexeme, Allocator>&, const widechar*, size_t ) const -> std::vector<Lexeme, Allocator>&;
  template <class Allocator>
    auto  MakeImage( BaseImage<Allocator>&, const textAPI::ITextView&, const FieldHandler* = nullptr ) const -> BaseImage<Allocator>&;
  template <class Allocator>
    auto  SetMarkup( BaseImage<Allocator>&, const textAPI::ITextView& ) const -> BaseImage<Allocator>&;
  template <class Allocator>
    auto  WordBreak( BaseImage<Allocator>&, const textAPI::ITextView&, const FieldHandler* = nullptr ) const -> BaseImage<Allocator>&;

    auto  Lemmatize( const mtc::widestr& ) const -> std::vector<Lexeme>;
    auto  MakeImage( const textAPI::ITextView&, const FieldHandler* = nullptr ) const -> Image;
    auto  WordBreak( const textAPI::ITextView&, const FieldHandler* = nullptr ) const -> Image;

  public:
    auto  Initialize( unsigned langId, const mtc::api<ILemmatizer>& ) -> Processor&;
    auto  Initialize( const Slice<const std::pair<unsigned, const mtc::api<ILemmatizer>>>& ) ->Processor&;

  protected:
    static  bool  IsPunct( widechar c )
    {
      return (codepages::charType[c] & 0xf0) == codepages::cat_P
          || (codepages::charType[c] & 0xf0) == codepages::cat_S;
    }
    void  MapMarkup(
      const Slice<textAPI::MarkupTag>&,
      const Slice<const textAPI::TextToken>& ) const;

  protected:
    std::vector<Lemmatizer>  languages;

  };

  template <class Allocator>
  class Processor::InsertTerms: public ILemmatizer::IWord
  {
    implement_lifetime_stub

    void  AddTerm( uint32_t lex, float, const uint8_t* forms, size_t count ) override
    {
      lemmas.emplace_back( langId, lex );
      lemmas.back().GetForms().set( forms, count );
    }
    void  AddStem( const widechar* pws, size_t len, uint32_t cls, float, const uint8_t* forms, size_t count ) override
    {
      lemmas.emplace_back( langId, cls, pws, len, lemmas.get_allocator() );
      lemmas.back().GetForms().set( forms, count );
    }

  public:
    InsertTerms( std::vector<Lexeme, Allocator>& terms, unsigned ilang ):
      lemmas( terms ),
      langId( ilang ) {}
    auto  ptr() const -> IWord* {  return (IWord*)this;  }

  protected:
    std::vector<Lexeme, Allocator>& lemmas;
    unsigned                        langId;

  };

  // Processor template implementation

  template <class Allocator>
  auto  Processor::SetMarkup( BaseImage<Allocator>& image,
    const textAPI::ITextView& input ) const -> BaseImage<Allocator>&
  {
    image.markup.insert( image.markup.end(),
      input.GetMarkup().begin(), input.GetMarkup().end() );

    return MapMarkup( image.GetMarkup(), image.GetTokens() ), image;
  }

  template <class Allocator>
  auto  Processor::Lemmatize( BaseImage<Allocator>& image ) const -> BaseImage<Allocator>&
  {
    auto  strmap = mtc::arbitrarymap<std::vector<int>>();

    image.lemmas.clear();
    image.lemmas.resize( image.tokens.size() );
    image.lexbuf.reserve( image.tokens.size() * 2 );

  // create words index
    for ( size_t i = 0; i < image.tokens.size(); i++ )
    {
      auto& rfword = image.tokens[i];
      auto  pfound = strmap.Search( rfword.pwsstr, sizeof(widechar) * rfword.length );

      if ( pfound == nullptr )
        pfound = strmap.Insert( rfword.pwsstr, sizeof(widechar) * rfword.length );

      pfound->push_back( i );
    }

  // lemmatize the words
    for ( auto next = strmap.Enum( nullptr ); next != nullptr; next = strmap.Enum( next ) )
    {
      auto  keystr = strmap.GetKey( next );
      auto  keylen = strmap.KeyLen( next );
      auto& values = strmap.GetVal( next );
      auto  curlen = image.lexbuf.size();

    // lemmatize next word
      Lemmatize( image.lexbuf, (const widechar*)keystr, keylen / sizeof(widechar) );

    // register word reference(s)
      for ( auto& i: values )
        new( &image.lemmas[i] ) Slice<Lexeme>( (Lexeme*)curlen, image.lexbuf.size() - curlen );
    }

  // transform indexes to pointers
    for ( auto& l: image.lemmas )
      l = { image.lexbuf.data() + size_t(l.data()), l.size() };

    return image;
  }

  template <class Allocator>
  auto  Processor::Lemmatize( std::vector<Lexeme, Allocator>& buf, const widechar* str, size_t len ) const -> std::vector<Lexeme, Allocator>&
  {
    auto  curlen = buf.size();

  // lemmatize with languages
    for ( auto& lang: languages )
      lang.module->Lemmatize( InsertTerms( buf, lang.langId ).ptr(), str, len );

  // check for hieroglyph
    if ( buf.size() == curlen )
      buf.push_back( Lexeme( 0xff, codepages::strtolower( str, len ), buf.get_allocator() ) );

    return buf;
  }

 /*
  * word breaker
  *
  * stores words with context flags, pointer, offset and length to output array
  */
  template <class Allocator>
  auto  Processor::WordBreak( BaseImage<Allocator>& body, const textAPI::ITextView& input,
    const FieldHandler* fdset ) const -> BaseImage<Allocator>&
  {
    std::vector<uint64_t>  nonBrk;
    uint32_t               offset = 0;

    body.clear();

    // create non-breakable limits
    if ( fdset != nullptr )
      for ( auto& markup: input.GetMarkup() )
      {
        auto  pfield = fdset->Get( markup.format );

        if ( pfield != nullptr )
        {
          if ( (pfield->options & FieldOptions::ofNoBreakWords) != 0 )
            mtc::bitset_set( nonBrk, { markup.uLower, markup.uUpper } );
          else
            mtc::bitset_del( nonBrk, { markup.uLower, markup.uUpper } );
        }
      }

    // list all blocks
    for ( auto beg = input.GetBlocks().begin(); beg != input.GetBlocks().end(); offset += (beg++)->length )
    {
      auto  sblock = beg->GetWideStr();
      auto  buforg = body.AddBuffer( sblock.data(), sblock.size() );
      auto  ptrtop = buforg;
      auto  ptrend = ptrtop + sblock.size();

      for ( unsigned uFlags = textAPI::TextToken::is_first; ptrtop != ptrend; uFlags = 0 )
      {
        // detect lower space
        if ( codepages::IsBlank( *ptrtop ) )
        {
          uFlags |= textAPI::TextToken::lt_space;

          for ( ++ptrtop; ptrtop != ptrend && codepages::IsBlank( *ptrtop ); ++ptrtop )
            (void)NULL;
        }

        // select next word
        if ( ptrtop != ptrend )
        {
          auto  origin = ptrtop;

          // check non-breakable limits
          if ( mtc::bitset_get( nonBrk, offset + (ptrtop - buforg) ) )
          {
            do ++ptrtop;
              while ( ptrtop != ptrend && mtc::bitset_get( nonBrk, offset + (ptrtop - origin) ) );
          }
            else
          // get substring length
          if ( IsPunct( *ptrtop++ ) )
          {
            uFlags |= textAPI::TextToken::is_punct;
          }
            else
          // select next word
          {
            while ( ptrtop != ptrend && !codepages::IsBlank( *ptrtop ) && !IsPunct( *ptrtop ) )
              ++ptrtop;
          }

          // create word string
          body.GetTokens().push_back( { uFlags, origin,
            uint32_t(offset + origin - buforg), uint32_t(ptrtop - origin) } );
        }
      }
    }
    return body;
  }

  template <class Allocator>
  auto  Processor::MakeImage( BaseImage<Allocator>& body, const textAPI::ITextView& text,
    const FieldHandler* fdset ) const -> BaseImage<Allocator>&
  {
    return SetMarkup( Lemmatize( WordBreak( body, text, fdset ) ), text );
  }

}}

# endif // !__DelphiX_context_processor_hpp__
