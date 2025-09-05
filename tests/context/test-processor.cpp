# include "../../context/processor.hpp"
# include "../../textAPI/DOM-dump.hpp"
# include <mtc/test-it-easy.hpp>

using namespace DelphiX;

template <>
auto  Serialize( std::string* to, const void* s, size_t len ) -> std::string*
{
  return to->append( (const char*)s, len ), to;
}

class MockLang: public ILemmatizer
{
  implement_lifetime_stub

  int   Lemmatize( IWord* word, const widechar* pstr, size_t ncch ) override
  {
    auto  stem = codepages::strtolower( pstr, ncch );

    word->AddStem( stem.c_str(), ncch >= 3 ? 3 : 1, 1U, 1.0,
      std::initializer_list<uint8_t>{ 1, 2, 3 }.begin() , 3 );
    return 0;
  }
};

class MockFields: public FieldHandler
{
  auto  Add( const StrView& ) -> FieldOptions* override
  {
    throw std::invalid_argument( "unexpected call @" __FILE__ ":" LINE_STRING );
  }
  auto  Get( const StrView& name ) const -> const FieldOptions* override
  {
    if ( name == "tag-1" )
      return &tag_1;
    if ( name == "tag-2" )
      return &tag_2;
    return nullptr;
  }
  auto  Get( unsigned ) const -> const FieldOptions* override
  {
    throw std::runtime_error("MockFields::Get( id ) not implemented");
  }

protected:
  FieldOptions  tag_1 = {
    1, "tag-1", 1.0, FieldOptions::ofNoBreakWords };
  FieldOptions  tag_2 = {
    2, "tag-2", 1.0 };
};

TestItEasy::RegisterFunc  test_processor( []()
{
  TEST_CASE( "context/processor" )
  {
    MockLang            mockLg;
    MockFields          mockFd;
    context::Processor  txProc;
    context::Image      txBody;
    textAPI::Document   ucText;

    CopyUtf16( &ucText, textAPI::Document{
      "Первая строка текста: просто строка",
      "Вторая строка в новом абзаце",
      { "tag-1", {
        "Строка внутри тега",
        { "tag-2", {
          "Строка внутри вложенного тега" } } } },
      "Третья строка." } );

    SECTION( "it works only with utf16 texts" )
    {
      REQUIRE_EXCEPTION( txProc.WordBreak( txBody, textAPI::Document{ "some string" } ),
        std::invalid_argument );
    }
    SECTION( "utf-16 text may be tokenized" )
    {
      REQUIRE_NOTHROW( txProc.WordBreak( txBody, ucText ) );

      REQUIRE( txBody.GetTokens().size() == 21 );

      SECTION( "formatter may affect word breaking" )
      {
        if ( REQUIRE_NOTHROW( txProc.WordBreak( txBody, ucText, &mockFd ) ) )
        {
          REQUIRE( txBody.GetTokens().size() == 19 );
        }
      }
    }
    SECTION( "formatting may be transfered from text to BaseBody" )
    {
      REQUIRE_NOTHROW( txProc.SetMarkup( txBody, ucText ) );
    }
    SECTION( "text body may be printed" )
    {
      auto  dump = std::string();

      if ( REQUIRE_NOTHROW( txBody.Serialize( textAPI::dump_as::Tags( textAPI::dump_as::MakeOutput( &dump ) ).ptr() ) ) )
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
          "    Строка внутри тега\n"
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
    SECTION( "text image may be lemmatized" )
    {
      SECTION( "* without language modules the hieroglyphs are created" )
      {
        if ( REQUIRE_NOTHROW( txProc.Lemmatize( txBody ) ) )
          for ( auto& next: txBody.GetLemmas() )
          {
            if ( REQUIRE( next.size() == 1 ) )
              REQUIRE( next.front().get_idl() == 0xff );
          }
      }
      SECTION( "* language modules may be added" )
      {
        REQUIRE_NOTHROW( txProc.Initialize( 0x7, &mockLg ) );

        SECTION( "with language modules it implements all the lexemes" )
        {
          if ( REQUIRE_NOTHROW( txProc.Lemmatize( txBody ) ) )
            for ( auto& next: txBody.GetLemmas() )
            {
              if ( REQUIRE( next.size() == 1 ) )
                REQUIRE( next.front().get_idl() == 0x7 );
            }
        }
      }
    }
  }
} );
