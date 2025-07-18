# include "textAPI/DOM-text.hpp"
# include "textAPI/DOM-dump.hpp"
# include "textAPI/DOM-load.hpp"
# include "textAPI/word-break.hpp"
# include <moonycode/codes.h>
# include <mtc/serialize.h>
# include <mtc/arena.hpp>
# include <mtc/test-it-easy.hpp>

template <>
auto  Serialize( std::string* to, const void* s, size_t len ) -> std::string*
{
  return to->append( (const char*)s, len ), to;
}

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

      SECTION( "* as json;" )
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
      SECTION( "* as tags;" )
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

      SECTION( "* as json;" )
      {
        REQUIRE_NOTHROW( load_as::Json( &text, load_as::MakeSource( json ) ) );

        REQUIRE( text.GetBlocks().size() == 6 );
        REQUIRE( text.GetMarkup().size() == 2 );
      }

      SECTION( "* as tags;" )
      {
        text.clear();
        REQUIRE_NOTHROW( load_as::Tags( &text, load_as::MakeSource( tags ) ) );

        REQUIRE( text.GetBlocks().size() == 10 );
        REQUIRE( text.GetMarkup().size() == 3 );
      }

      SECTION( "* as dump;" )
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
    auto  txBody = BaseBody<std::allocator<char>>();

    SECTION( "multibyte text could not be processed by tokenizer" )
    {
      REQUIRE_EXCEPTION( BreakWords( txBody, inText.GetBlocks() ),
        std::logic_error );
    }
    SECTION( "any text may be converted to utf16" )
    {
      REQUIRE_NOTHROW( inText.CopyUtf16( &ucText ) );

      REQUIRE( ucText.GetMarkup().size() == inText.GetMarkup().size() );
      REQUIRE( ucText.GetBlocks().size() == inText.GetBlocks().size() );

      for ( auto& next: ucText.GetBlocks() )
        REQUIRE( next.encode == unsigned(-1) );
    }
    SECTION( "utf-16 text may be tokenized" )
    {
      REQUIRE_NOTHROW( BreakWords( txBody, ucText.GetBlocks() ) );
    }
    SECTION( "formatting may be transfered from text to BaseBody" )
    {
      REQUIRE_NOTHROW( CopyMarkup( txBody, ucText.GetMarkup() ) );
    }
    SECTION( "text body may be printed" )
    {
      auto  dump = std::string();

      if ( REQUIRE_NOTHROW( txBody.Serialize( dump_as::Tags( dump_as::MakeOutput( &dump ) ).ptr() ) ) )
      {
        REQUIRE( dump ==
          "Первая\n"
          "строка\n"
          "текста\n"
          ":\n"
          "просто\n"
          "строка\n"
          "Вторая\n"
          "строка\n"
          "в\n"
          "новом\n"
          "абзаце\n"
          "  <tag-1>\n"
          "    Строка\n"
          "    внутри\n"
          "    тега\n"
          "    <tag-2>\n"
          "      Строка\n"
          "      внутри\n"
          "      вложенного\n"
          "      тега\n"
          "    </tag-2>\n"
          "  </tag-1>\n"
          "Третья\n"
          "строка\n"
          ".\n" );
      }
    }
  }
} );
