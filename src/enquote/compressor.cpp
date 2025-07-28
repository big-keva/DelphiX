# include "../../enquote/compressor.hpp"
# include <mtc/arbitrarymap.h>
# include <functional>

template <> inline
auto  Serialize( std::vector<char>* to, const void* p, size_t l ) -> std::vector<char>*
{
  return to->insert( to->end(), (const char*)p, (const char*)p + l ), to;
}

template <> inline
auto  Serialize( std::function<void(const void*, size_t)>* to, const void* p, size_t l ) -> std::function<void(const void*, size_t)>*
{
  return to != nullptr ? (*to)( p, l ), to : nullptr;
}

namespace DelphiX {
namespace enquote {

  class UtfStrBuffer: protected std::vector<widechar>
  {
    widechar  buffer[0x100];

  public:
    auto  GetBuffer( size_t len ) -> widechar*
    {
      if ( len <= std::size(buffer) )
        return buffer;
      if ( len > size() )
        resize( len );
      return data();
    }
  };
  class WordsEncoder final
  {
    mtc::arbitrarymap<unsigned> references;

  public:
    enum: unsigned
    {
      of_1251str = 0x0,
      of_utf8str = 0x4,
      of_backref = 0x8,
      of_diffref = 0xc,
      of_bitmask = 0xc
    };

    template <class O>
    auto  EncodeWord( O* o, const textAPI::TextToken& t, unsigned p ) -> O*
    {
      auto  puprev = references.Search( t.pwsstr, t.length * sizeof(widechar) );

      if ( puprev != nullptr )
      {
        auto  asOffs = AsOffs( t, p );
        auto  asDiff = AsDiff( t, *puprev );
        auto  ccOffs = ::GetBufLen( asOffs );
        auto  ccDiff = ::GetBufLen( asDiff );

      // get min backref
        if ( ccDiff < ccOffs )  o = ::Serialize( o, asDiff );
          else o = ::Serialize( o, ccOffs );

        return *puprev = p, o;
      }

      if ( Is1251( t ) )
      {
        o = ::Serialize( o, As1251( t ) );

        for ( auto p = t.pwsstr, e = p + t.length; p != e; ++p )
          o = ::Serialize( o, uint8_t(codepages::__impl__::utf_1251<>::translate( *p )) );
      }
        else
      {
        o = ::Serialize( o, AsUtf8( t ) );

        for ( auto p = t.pwsstr, e = p + t.length; p != e; ++p )
        {
          char  encode[8];
          auto  cchenc = codepages::__impl__::utf::encodechar( encode, sizeof(encode), *p );

          o = ::Serialize( o, encode, cchenc );
        }
      }
      references.Insert( t.pwsstr, t.length, p );
      return o;
    }

  protected:
    static  bool  Is1251( const textAPI::TextToken& t )
      {
        for ( auto p = t.pwsstr, e = p + t.length; p != e; ++p )
          if ( codepages::xlatWinToUtf16[codepages::xlatUtf16ToWin[*p >> 8][*p & 0xff]] != *p )
            return false;
        return true;
      }
    static  auto  As1251( const textAPI::TextToken& t ) -> unsigned
      {  return t.uFlags + ((t.length - 1) << 4);  }
    static  auto  AsUtf8( const textAPI::TextToken& t ) -> unsigned
      {  return t.uFlags + ((t.length - 1) << 4) + of_utf8str ;  }
    static  auto  AsDiff( const textAPI::TextToken& t, unsigned diff ) -> unsigned
      {  return t.uFlags + ((diff - 1) << 4) + of_diffref ;  }
    static  auto  AsOffs( const textAPI::TextToken& t, unsigned next ) -> unsigned
      {  return t.uFlags + ((next - 1) << 4) + of_backref ;  }
  };

  template <class O>
  void  PackWordsTo( O* o, const Slice<const textAPI::TextToken>& words )
  {
    WordsEncoder      wcoder;

    ::Serialize( o, words.size() );

    for ( size_t pos = 0; pos != words.size(); ++pos )
      wcoder.EncodeWord( o, words[pos], pos );
  }

  auto  PackWords( const Slice<const textAPI::TextToken>& words ) -> std::vector<char>
  {
    std::vector<char> packed;
      PackWordsTo( &packed, words );
    return packed;
  }

  void  PackWords( mtc::IByteStream* o, const Slice<const textAPI::TextToken>& words )
  {
    return PackWordsTo( o, words );
  }

  void  PackWords( std::function<void(const void*, size_t)> fn, const Slice<const textAPI::TextToken>& words )
  {
    return PackWordsTo( &fn, words );
  }

  void  UnpackWords(
    std::function<void(unsigned, const Slice<const widechar>&)> addstr,
    std::function<void(unsigned, unsigned)>                     addref,
    const Slice<const char>&                                    packed )
  {
    auto  src = mtc::sourcebuf( packed.data(), packed.size() );
    auto  inp = src.ptr();
    auto  buf = UtfStrBuffer();
    int   len;

    if ( (inp = ::FetchFrom( inp, len )) == nullptr )
      throw std::invalid_argument( "broken text image" );

    for ( int pos = 0; pos != len; ++pos )
    {
      unsigned  opt;

      if ( (inp = ::FetchFrom( inp, opt )) == nullptr )
        throw std::invalid_argument( "broken text image" );

      switch ( opt & WordsEncoder::of_bitmask )
      {
        case WordsEncoder::of_backref:
          addref( opt & 0x3, 1 + (opt >> 4) );
          break;
        case WordsEncoder::of_diffref:
          addref( opt & 0x3, pos - (1 + (opt >> 4)) );
          break;
        case WordsEncoder::of_utf8str:
        {
          auto  len = 1 + (opt >> 4);
          auto  str = inp->getptr();

          if ( (inp = ::SkipBytes( inp, len )) == nullptr )
            throw std::invalid_argument( "broken text image" );

          auto  cch = codepages::utf8::strlen( str, len );
          auto  wcs = buf.GetBuffer( cch + 1 );

          codepages::utf8::decode( wcs, cch + 1, str, len );
          addstr( opt & 0x3, { wcs, len } );
          break;
        }
        case WordsEncoder::of_1251str:  default:  // 0
        {
          auto  len = 1 + (opt >> 4);
          auto  wcs = buf.GetBuffer( len );
          auto  str = inp->getptr();
          auto  out = wcs;

          if ( (inp = ::SkipBytes( inp, len )) == nullptr )
            throw std::invalid_argument( "broken text image" );

          for ( auto beg = str, end = beg + len; beg != end; )
            *out++ = codepages::xlatWinToUtf16[(uint8_t)*beg++];

          addstr( opt & 0x3, { wcs, len } );
          break;
        }
      }
    }
  }

  auto  UnpackWords( const Slice<const char>& packed ) -> context::Image
  {
    auto  body = context::Image();

    UnpackWords(
      [&]( unsigned uflags, const Slice<const widechar>& inp )
      {
        body.GetTokens().push_back( {
          uflags,
          body.AddBuffer( inp.data(), inp.size() ),
          unsigned(body.GetTokens().size()),
          unsigned(inp.size()) } );
      },
      [&]( unsigned uflags, unsigned pos )
      {
        if ( pos >= body.GetTokens().size() )
          throw std::invalid_argument( "broken text image - invalid reference" );
        body.GetTokens().push_back(
          body.GetTokens()[pos] );
        body.GetTokens().back().uFlags = uflags;
      }, packed );
    return body;
  }

}}
