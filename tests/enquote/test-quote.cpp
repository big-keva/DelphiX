# include "../../context/pack-images.hpp"
# include "../../context/pack-format.hpp"
# include "../../context/processor.hpp"
# include "../../queries.hpp"
# include "../../enquote/quotations.hpp"
# include <DeliriX/DOM-dump.hpp>
# include <mtc/test-it-easy.hpp>

using namespace DelphiX;

auto  GetPackedImage( const DeliriX::Text& doc, FieldHandler& fds ) -> std::pair<std::vector<char>, std::vector<char>>
{
  auto  ucText = DeliriX::Text();
  auto  txBody = context::BaseImage<std::allocator<char>>();
  auto  lgProc = context::Processor();

  CopyUtf16( &ucText, doc );

  lgProc.WordBreak( txBody, ucText );
  lgProc.SetMarkup( txBody, ucText );

  return {
    context::imaging::Pack( txBody.GetTokens() ),
    context::formats::Pack( txBody.GetMarkup(), fds ) };
}

auto  GetUnpack( const std::pair<std::vector<char>, std::vector<char>>& image, const FieldHandler& fdhan ) -> context::Image
{
  auto  ximage = context::Image();
  auto  addTag = [&]( const context::formats::RankerTag& tag )
    {
      auto  pf = fdhan.Get( tag.format );

      if ( pf != nullptr )
        ximage.GetMarkup().push_back( { pf->name.data(), tag.uLower, tag.uUpper } );
    };

  context::imaging::Unpack( ximage, image.first );
  context::formats::Unpack( addTag, image.second );

  return ximage;
}

class FdMan: public FieldHandler
{
public:
  FieldOptions  tags[3] = {
    { 1, "tag-1" },
    { 2, "tag-2" },
    { 3, "tag-3" } };

public:
  auto  Add( const std::string_view& tag ) -> FieldOptions* override
  {
    for ( auto& next: tags )
      if ( tag == next.name )
        return &next;
    return nullptr;
  }
  auto  Get( const std::string_view& tag ) const -> const FieldOptions* override
  {
    for ( auto& next: tags )
      if ( tag == next.name )
        return &next;
    return nullptr;
  }
  auto  Get( unsigned id ) const -> const FieldOptions* override
  {
    for ( auto& next: tags )
      if ( id == next.id )
        return &next;
    return nullptr;
  }
};

auto  UTF8( const std::basic_string_view<widechar>& s ) -> mtc::charstr
{
  return codepages::widetombcs( codepages::codepage_utf8, s );
}

TestItEasy::RegisterFunc  test_quote( []()
{
  TEST_CASE( "enquote/quote" )
  {
    FdMan fd_man;
    auto  inText = DeliriX::Text();

    DeliriX::CopyUtf16( &inText, DeliriX::Text{
      "Первая строка текста: просто строка,",
      "вторая строка в новом абзаце",
      { "tag-1", {
        "Строка внутри тега",
        { "tag-2", {
          "Строка внутри вложенного тега" } } } },
      { "tag-3", { "Текст, что надо всегда цитировать" } },
      "Третья строка очень длинная и состоит из множества разных слов и букв для проявления многоточий в цитировании" }, codepages::codepage_utf8 );
    auto  packed = GetPackedImage( inText, fd_man );
    auto  quoter = DelphiX::enquote::QuoteMachine( fd_man )
      .SetIndent( { { 2, 4 }, { 2, 4 } } );

    auto  Quoter = [&]( const queries::Abstract& quotes = {} ) -> DeliriX::Text
      {
        auto  output = DeliriX::Text();
          quoter.Structured()( &output, packed.first, packed.second, quotes );
        return output;
      };
    auto  Source = [&]( const queries::Abstract& quotes = {} ) -> DeliriX::Text
      {
        auto  output = DeliriX::Text();
          quoter.TextSource()( &output, packed.first, packed.second, quotes );
        return output;
      };

    SECTION( "image text may be reconstructed" )
    {
      auto  quoted = Source( {} );

      if ( REQUIRE( quoted.GetBlocks().size() == inText.GetBlocks().size() ) )
        for ( size_t i = 0; i != inText.GetBlocks().size(); ++i )
          REQUIRE( quoted.GetBlocks().at( i ).GetWideStr() == inText.GetBlocks().at( i ).GetWideStr() );

      if ( REQUIRE( quoted.GetMarkup().size() == inText.GetMarkup().size() ) )
        for ( size_t i = 0; i != inText.GetMarkup().size(); i++ )
          REQUIRE( quoted.GetMarkup().at( i ) == inText.GetMarkup().at( i ) );

      SECTION( "if quotation data is passed, words are marked" )
      {
        auto  membuf = mtc::Arena();
        auto  quotes = queries::MakeAbstract( membuf, {
          queries::MakeEntrySet( membuf, { { 0, 1 }, { 1, 2 } } ),
          queries::MakeEntrySet( membuf, { { 2, 13 }, { 3, 14 } } ) } );

        quoted = Quoter( quotes );

        if ( REQUIRE( quoted.GetBlocks().size() == 2 ) )
        {
          REQUIRE( UTF8( quoted.GetBlocks()[0].GetWideStr() ) == "Первая \x7строка\x8 \x7текста\x8: просто строка," );
          REQUIRE( UTF8( quoted.GetBlocks()[1].GetWideStr() ) == "Строка \x7внутри\x8 \x7тега\x8" );
        }
      }
    }

    SECTION( "image text may be quoted" )
    {
      auto  membuf = mtc::Arena();
      auto  quoted = DeliriX::Text();

      SECTION( "without markup it is empty document" )
      {
        quoted = Quoter( {} );

        REQUIRE( quoted.GetBlocks().empty() );
      }
      SECTION( "with always-quoted field it is the set of always-quoteds" )
      {
        fd_man.tags[2].options |= FieldOptions::ofEnforceQuote;

        quoted = Quoter( {} );

        if ( REQUIRE( quoted.GetBlocks().size() == 1 ) )
          REQUIRE( UTF8( quoted.GetBlocks().front().GetWideStr() ) == "Текст, что надо всегда цитировать" );
        if ( REQUIRE( quoted.GetMarkup().size() == 1 ) )
          REQUIRE( std::string_view( quoted.GetMarkup().front().tagKey ) == "tag-3" );

        fd_man.tags[2].options &= ~FieldOptions::ofEnforceQuote;
      }

      SECTION( "with quotation data, it selects fragments" )
      {
        auto  quotes = queries::MakeAbstract( membuf, {
          queries::MakeEntrySet( membuf, { { 0, 1 }, { 1, 2 } } ),
          queries::MakeEntrySet( membuf, { { 2, 12 }, { 3, 13 } } ) } );

        quoted = Quoter( quotes );

        if ( !REQUIRE( quoted.GetBlocks().size() == 2 )
          || !REQUIRE( UTF8( quoted.GetBlocks()[0].GetWideStr() ) == "Первая \x7строка\x8 \x7текста\x8: просто строка," )
          || !REQUIRE( UTF8( quoted.GetBlocks()[1].GetWideStr() ) == "\x7Строка\x8 \x7внутри\x8 тега" ) )
        {
          quoted.Serialize( DeliriX::dump_as::Json( DeliriX::dump_as::MakeOutput( stdout ) ) );
        }

        SECTION( "always-quoted field will present always" )
        {
          fd_man.tags[2].options |= FieldOptions::ofEnforceQuote;

          quoted = Quoter( quotes );
//            .SetLabels( "<span id=%u>", "</span>" )

          if ( !REQUIRE( quoted.GetBlocks().size() == 3 )
            || !REQUIRE( UTF8( quoted.GetBlocks()[0].GetWideStr() ) == "Первая \x7строка\x8 \x7текста\x8: просто строка," )
            || !REQUIRE( UTF8( quoted.GetBlocks()[1].GetWideStr() ) == "\x7Строка\x8 \x7внутри\x8 тега" )
            || !REQUIRE( UTF8( quoted.GetBlocks()[2].GetWideStr() ) == "Текст, что надо всегда цитировать" ) )
          {
            quoted.Serialize( DeliriX::dump_as::Json( DeliriX::dump_as::MakeOutput( stdout ) ) );
          }

          fd_man.tags[2].options &= ~FieldOptions::ofEnforceQuote;
        }

        SECTION( "long events are quoted partially")
        {
          quotes = queries::MakeAbstract( membuf, {
            queries::MakeEntrySet( membuf, { { 0, 30 }, { 1, 41 } } ) } );

          quoted = Quoter( quotes );

          if ( !REQUIRE( quoted.GetBlocks().size() == 2 )
            || !REQUIRE( UTF8( quoted.GetBlocks()[0].GetWideStr() ) == "… строка очень длинная и \x7состоит\x8 из множества разных слов…" )
            || !REQUIRE( UTF8( quoted.GetBlocks()[1].GetWideStr() ) == "… для проявления многоточий в \x7цитировании\x8" ) )
          {
            quoted.Serialize( DeliriX::dump_as::Json( DeliriX::dump_as::MakeOutput( stdout ) ) );
          }
        }
        SECTION( "multiple events in one block are supported" )
        {
          quotes = queries::MakeAbstract( membuf, {
            queries::MakeEntrySet( membuf, { { 0, 33 } } ),
            queries::MakeEntrySet( membuf, { { 1, 39 } } ) } );

          quoted = Quoter( quotes );

          if ( !REQUIRE( quoted.GetBlocks().size() == 1 )
            || !REQUIRE( UTF8( quoted.GetBlocks()[0].GetWideStr() ) == "… и состоит из множества \x7разных\x8 слов и букв для проявления \x7многоточий\x8 в цитировании" ) )
          {
            quoted.Serialize( DeliriX::dump_as::Json( DeliriX::dump_as::MakeOutput( stdout ) ) );
          }
        }
      }

      SECTION( "with non-quote abstract it creates start document snapshot" )
      {
        quoted = Quoter( {} );

        quoted.Serialize( DeliriX::dump_as::Json( DeliriX::dump_as::MakeOutput( stdout ) ) );
      }
    }
  }

} );
