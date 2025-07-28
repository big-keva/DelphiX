# include "../../textAPI/DOM-text.hpp"
# include "../../textAPI/DOM-dump.hpp"
# include "../../textAPI/DOM-load.hpp"
# include <moonycode/codes.h>
# include <mtc/serialize.h>
# include <mtc/arena.hpp>
# include <mtc/test-it-easy.hpp>
# include <mtc/iStream.h>

template <>
auto  Serialize( std::string* to, const void* s, size_t len ) -> std::string*
{
  return to->append( (const char*)s, len ), to;
}

class ByteStreamOnString: public mtc::IByteStream
{
  std::string& str;

  implement_lifetime_stub

public:
  ByteStreamOnString( std::string& s ): str( s )
    {}
  uint32_t  Get(       void*, uint32_t ) override
    {  throw std::logic_error("not implemented");  }
  uint32_t  Put( const void* p, uint32_t l ) override
    {  str += std::string( (const char*)p, l );  return l;  }
  auto  ptr() const -> IByteStream*
    {  return (IByteStream*)this;  }
};

using namespace DelphiX;
using namespace DelphiX::textAPI;

const char  json[] =
  "[\n"
  "  \"aaa\",\n"
  "  \"bbb\",\n"
  "  { \"body\": [\n"
  "    \"first line in <body>\",\n"
  "    { \"p\": [\n"
  "      \"first line in <p> tag\",\n"
  "    ] },\n"
  "    \"third line in <body>\",\n"
  "  ] },\n"
  "  \"last text line \\\"with quotes\\\"\""
  "]\n";

const char  tags[] =
  "aaa\n"
  "bbb\n"
  "<body>\n"
  "  first line in &lt;body&gt;\n"
  "  second line in &lt;body&gt;\n"
  "<p>\n"
    "first line in &lt;p&gt; tag\n"
    "second line in &lt;p&gt; tag\n"
  "</p>\n"
  "third line in &lt;body&gt;\n"
  "<span>\n"
    "text line in &lt;span&gt;\n"
  "</span>\n"
  "fourth line in &lt;body&gt;\n"
  "</body>\n"
  "last text line \"with quotes\"";

# include <mtc/arbitrarymap.h>

template <>
auto  Serialize( std::vector<char>* to, const void* p, size_t l ) -> std::vector<char>*
{
  return to->insert( to->end(), (const char*)p, (const char*)p + l ), to;
}

class WordsEncoder final
{
  mtc::arbitrarymap<unsigned> references;

public:
  enum: unsigned
  {
    of_backref = 0x8,
    of_diffref = 0xc,
    of_utf8str = 0x4,
    of_bitmask = 0xc
  };

  auto  EncodeWord( std::vector<char>& o, const TextToken& t, unsigned p ) -> std::vector<char>&
  {
    auto  puprev = references.Search( t.pwsstr, t.length * sizeof(widechar) );

    if ( puprev != nullptr )
    {
      auto  asOffs = AsOffs( t, p );
      auto  asDiff = AsDiff( t, *puprev );
      auto  ccOffs = ::GetBufLen( asOffs );
      auto  ccDiff = ::GetBufLen( asDiff );

    // get min backref
      if ( ccDiff < ccOffs )  ::Serialize( &o, asDiff );
        else ::Serialize( &o, ccOffs );

      return *puprev = p, o;
    }

    if ( Is1251( t ) )
    {
      ::Serialize( &o, As1251( t ) );

      for ( auto p = t.pwsstr, e = p + t.length; p != e; ++p )
        ::Serialize( &o, uint8_t(codepages::__impl__::utf_1251<>::translate( *p )) );
    }
      else
    {
      ::Serialize( &o, AsUtf8( t ) );

      for ( auto p = t.pwsstr, e = p + t.length; p != e; ++p )
      {
        char  encode[8];
        auto  cchenc = codepages::__impl__::utf::encodechar( encode, sizeof(encode), *p );

        ::Serialize( &o, encode, cchenc );
      }
    }
    references.Insert( t.pwsstr, t.length, p );
    return o;
  }

protected:
  static  bool  Is1251( const TextToken& t )
    {
      for ( auto p = t.pwsstr, e = p + t.length; p != e; ++p )
        if ( codepages::xlatWinToUtf16[codepages::xlatUtf16ToWin[*p >> 8][*p & 0xff]] != *p )
          return false;
      return true;
    }
  static  auto  As1251( const TextToken& t ) -> unsigned
    {  return t.uFlags + ((t.length - 1) << 4);  }
  static  auto  AsUtf8( const TextToken& t ) -> unsigned
    {  return t.uFlags + ((t.length - 1) << 4) + of_utf8str ;  }
  static  auto  AsDiff( const TextToken& t, unsigned diff ) -> unsigned
    {  return t.uFlags + ((diff - 1) << 4) + of_diffref ;  }
  static  auto  AsOffs( const TextToken& t, unsigned next ) -> unsigned
    {  return t.uFlags + ((next - 1) << 4) + of_backref ;  }
};

TestItEasy::RegisterFunc  test_texts( []()
{
  TEST_CASE( "texts/DOM" )
  {
    auto  dump = std::string();

    SECTION( "BaseDocument implements mini-DOM interface IText" )
    {
      auto  text = Document();

      SECTION( "it supports text blocks and block markups" )
      {
        SECTION( "text blocks ade added to the end of image as strings or widestrings" )
        {
          if ( REQUIRE_NOTHROW( text.AddString( "aaa" ) ) )
            if ( REQUIRE( text.GetBlocks().size() == 1 ) )
              REQUIRE( text.GetBlocks().back().GetCharStr() == "aaa" );
          if ( REQUIRE_NOTHROW( text.AddString( { "bbbb", 3 } ) ) )
            if ( REQUIRE( text.GetBlocks().size() == 2 ) )
              REQUIRE( text.GetBlocks().back().GetCharStr() == "bbb" );
          if ( REQUIRE_NOTHROW( text.AddString( codepages::mbcstowide( codepages::codepage_utf8, "ccc" ) ) ) )
            if ( REQUIRE( text.GetBlocks().size() == 3 ) )
            {
              REQUIRE_EXCEPTION( text.GetBlocks().back().GetCharStr(), std::logic_error );

              if ( REQUIRE_NOTHROW( text.GetBlocks().back().GetWideStr() ) )
                REQUIRE( text.GetBlocks().back().GetWideStr() == codepages::mbcstowide( codepages::codepage_utf8, "ccc" ) );
            }
        }
        SECTION( "text may be cleared" )
        {
          REQUIRE_NOTHROW( text.clear() );
          REQUIRE( text.GetBlocks().empty() );
        }
        SECTION( "tags cover lines and are closed automatcally" )
        {
          if ( REQUIRE_NOTHROW( text.AddString( "aaa" ) ) )
          {
            auto  tag = mtc::api<IText>();

            if ( REQUIRE_NOTHROW( tag = text.AddTextTag( "block" ) )
              && REQUIRE_NOTHROW( tag->AddString( "bbb" ) )
              && REQUIRE_NOTHROW( text.AddString( "ccc" ) ) )
            {
              if ( REQUIRE( text.GetMarkup().size() == 1 ) )
              {
                REQUIRE( strcmp( text.GetMarkup().back().format, "block" ) == 0 );
                REQUIRE( text.GetMarkup().back().uLower == 3 );
                REQUIRE( text.GetMarkup().back().uUpper == 5 );
              }
            }
          }
        }
      }
      SECTION( "it may be created with alternate allocator" )
      {
        auto  arena = mtc::Arena();
        auto  image = arena.Create<BaseDocument<mtc::Arena::allocator<char>>>();

        SECTION( "elements are allocated in arena with no need to deallocate" )
        {
          image->AddString( "This is a test string object to be allocated in memory arena" );
          image->AddString( codepages::mbcstowide( codepages::codepage_utf8,
            "This is a test widestring to be allocated in memory arena" ) );
        }
      }
    }
    SECTION( "BaseDocument may be serialized" )
    {
      auto  text = Document();

      text.AddString( "aaa" );
        text.AddMarkup( "bbb" )->AddString( "bbb" );
      text.AddString( "ccc" );

      SECTION( "* as json" )
      {
        if ( REQUIRE_NOTHROW( text.Serialize( dump_as::Json( dump_as::MakeOutput( &dump ) ).ptr() ) ) )
        {
          REQUIRE( dump ==
            "[\n"
            "  \"aaa\",\n"""
            "  { \"bbb\": [\n"
            "    \"bbb\"\n"
            "  ] },\n"
            "  \"ccc\"\n"
            "]" );
        }
      }
      SECTION( "* as tags" )
      {
        dump.clear();

        if ( REQUIRE_NOTHROW( text.Serialize( dump_as::Tags( dump_as::MakeOutput( &dump ) ).ptr() ) ) )
        {
          REQUIRE( dump ==
            "aaa\n"
            "  <bbb>\n"
            "    bbb\n"
            "  </bbb>\n"
            "ccc\n" );
        }
      }
      SECTION( "*as dump" )
      {
        dump.clear();

        REQUIRE_NOTHROW( text.Serialize( &dump ) );
        REQUIRE( dump.length() == text.GetBufLen() );
      }
    }
    SECTION( "BaseDocument may be loaded from source" )
    {
      auto  text = Document();

      SECTION( "* as json" )
      {
        REQUIRE_NOTHROW( load_as::Json( &text, load_as::MakeSource( json ) ) );

        REQUIRE( text.GetBlocks().size() == 6 );
        REQUIRE( text.GetMarkup().size() == 2 );
      }

      SECTION( "* as tags" )
      {
        text.clear();
        REQUIRE_NOTHROW( load_as::Tags( &text, load_as::MakeSource( tags ) ) );

        REQUIRE( text.GetBlocks().size() == 10 );
        REQUIRE( text.GetMarkup().size() == 3 );
      }

      SECTION( "* as dump" )
      {
        text.clear();
        REQUIRE_NOTHROW( text.FetchFrom( mtc::sourcebuf( dump ).ptr() ) );

        REQUIRE( text.GetBlocks().size() == 3 );
        REQUIRE( text.GetMarkup().size() == 1 );
      }
    }
    SECTION( "BaseDocument may be initialized with initializer list" )
    {
      auto  text = Document{
        "this is a first text string",
        { "tag", {
            "first tag string",
            "second tag string" } },
        "this is a second text string" };
      auto  outs = std::string();

      REQUIRE_NOTHROW( text.Serialize( dump_as::Json(
        dump_as::MakeOutput( &outs ) ) ) );
      REQUIRE( outs == "[\n"
        "  \"this is a first text string\",\n"
        "  { \"tag\": [\n"
        "    \"first tag string\",\n"
        "    \"second tag string\"\n"
        "  ] },\n"
        "  \"this is a second text string\"\n"
        "]" );
    }
  }
  TEST_CASE( "texts/word-break" )
  {
    auto  inText = Document{
      "Первая строка текста: просто строка",
      "Вторая строка в новом абзаце",
      { "tag-1", {
        "Строка внутри тега",
        { "tag-2", {
          "Строка внутри вложенного тега" } } } },
      "Третья строка."
    };
    auto  ucText = Document();

    SECTION( "any text may be converted to utf16" )
    {
      REQUIRE_NOTHROW( inText.CopyUtf16( &ucText ) );

      REQUIRE( ucText.GetMarkup().size() == inText.GetMarkup().size() );
      REQUIRE( ucText.GetBlocks().size() == inText.GetBlocks().size() );

      for ( auto& next: ucText.GetBlocks() )
        REQUIRE( next.encode == unsigned(-1) );
    }
  }
  /*
  TEST_CASE( "text/pack-unpack" )
  {
    SECTION( "array of TextToken may be packed" )
    {
      auto  ucText = Document();
      auto  txBody = BaseBody<std::allocator<char>>();

      Document{
        "Первая строка текста: просто строка",
        "Вторая строка в новом абзаце",
        { "tag-1", {
          "Строка внутри тега",
          { "tag-2", {
            "Строка внутри вложенного тега" } } } },
        "Третья строка." }.CopyUtf16( &ucText );
      BreakWords( txBody, ucText.GetBlocks() );

      auto  as_vec = std::vector<char>();
      auto  as_str = std::string();
      auto  as_stm = std::string();

      SECTION( "* as std::vector<char>" )
      {
        REQUIRE_NOTHROW( as_vec = PackWords( txBody.GetTokens() ) );
      }
      SECTION( "* as std::string through function<>" )
      {
        REQUIRE_NOTHROW( PackWords( [&]( const void* p, size_t l )
          {  as_str += std::string( (const char*)p, l );  }, txBody.GetTokens() ) );
      }
      SECTION( "* as IByteStream" )
      {
        REQUIRE_NOTHROW( PackWords( ByteStreamOnString( as_stm ).ptr(), txBody.GetTokens() ) );
      }
      SECTION( "and the results are the same" )
      {
        REQUIRE( as_str == as_stm );
        REQUIRE( as_stm.length() == as_vec.size() );
        REQUIRE( memcmp( as_stm.data(), as_vec.data(), as_stm.size() ) == 0 );
      }

      SECTION( "the packed words may be unpacked" )
      {
        auto  unpack = Body();

        if ( REQUIRE_NOTHROW( unpack = UnpackWords( as_vec ) ) )
          REQUIRE( unpack.GetTokens() == txBody.GetTokens() );
      }
    }
  }
  */
} );
