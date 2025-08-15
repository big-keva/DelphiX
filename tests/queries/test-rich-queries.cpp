# include "../../context/x-contents.hpp"
# include "../../context/processor.hpp"
# include "../../src/queries/rich-queries.hpp"
# include "../../indexer/dynamic-contents.hpp"
# include "../../textAPI/DOM-dump.hpp"
# include <mtc/test-it-easy.hpp>

using namespace DelphiX;
using namespace DelphiX::textAPI;

auto  _W( const char* s ) -> mtc::widestr
  {  return codepages::mbcstowide( codepages::codepage_utf8, s );  }

class MockFields: public FieldHandler
{
  auto  Get( const StrView& name ) const -> const FieldOptions* override
  {
    if ( name == "tag-1" )
      return &tag_1;
    if ( name == "tag-2" )
      return &tag_2;
    return nullptr;
  }
  auto  Add( const StrView& ) -> FieldOptions* override
  {
    return &tag_2;
  }
  auto  Get( unsigned ) const -> const FieldOptions* override
  {
    throw std::runtime_error( "MockFields::Get( id ) not implemented" );
  }

protected:
  FieldOptions  tag_1 = {
    1, "tag-1", 1.0, FieldOptions::ofNoBreakWords };
  FieldOptions  tag_2 = {
    2, "tag-2", 1.0 };
};

auto  MakeStats( const std::initializer_list<std::pair<const char*, double>>& init ) -> mtc::zmap
{
  auto  ts = mtc::zmap();

  for ( auto& term: init )
    ts.set_zmap( _W( term.first ), { { "range", term.second } } );
  return { { "stats", ts } };
}

auto  CreateRichIndex( const context::Processor& lp, const std::initializer_list<Document>& docs ) -> mtc::api<IContentsIndex>
{
  auto  ct = indexer::dynamic::Index().Create();
  auto  id = 0;
  auto  fm = MockFields();

  for ( auto& doc: docs )
  {
    auto  ucText = Document();
      CopyUtf16( &ucText, doc );
    auto  ucBody = context::Image();

    lp.SetMarkup(
    lp.Lemmatize(
    lp.WordBreak( ucBody, ucText ) ), ucText );

//    ucBody.Serialize( dump_as::Json( dump_as::MakeOutput( stdout ) ) );
    auto  ximage = GetRichContents( ucBody.GetLemmas(), ucBody.GetMarkup(), fm );
    ct->SetEntity( mtc::strprintf( "doc-%u", id++ ), ximage.ptr() );
  }
  return ct;
}

bool  MatchIds( mtc::api<queries::IQuery> query, const std::initializer_list<uint32_t>& ids )
{
  auto  found = query->SearchDoc( 1 );
  auto  idsIt = ids.begin();

  for ( ; idsIt != ids.end() && found != uint32_t(-1); ++idsIt, found = query->SearchDoc( found + 1 ) )
    if ( *idsIt != found )
      return false;
  return idsIt == ids.end() && found == uint32_t(-1);
}

TestItEasy::RegisterFunc  test_rich_queries( []()
{
  TEST_CASE( "context/rich-queries" )
  {
    auto  lp = context::Processor();
    auto  fm = MockFields();
    auto  xx = CreateRichIndex( lp, {
      { { "title", {
            "Сказ про радугу",
            "/",
            "Rainbow tales" } },
        { "body", {
            "Как однажды Жак Звонарь",
            "Городской сломал фонарь" } },
        "Очень старый фонарь" },

      { { "title", { "старый добрый Фонарь Диогена" } } },

      { "городской сумасшедший" } } );

    SECTION( "rich query may be created from it's structured representation" )
    {
      mtc::api<queries::IQuery> query;

      SECTION( "* invalid query causes std::invalid_argument" )
      {
        REQUIRE_EXCEPTION( query = queries::GetRichQuery( {}, {}, xx, lp, fm ), std::invalid_argument );
      }
      SECTION( "* for non-collection term the query is empty" )
      {
        if ( REQUIRE_NOTHROW( query = queries::GetRichQuery( { { "word", "Платон" } }, {}, xx, lp, fm ) ) )
          REQUIRE( query == nullptr );
      }
      SECTION( "* existing term produces a query" )
      {
        if ( REQUIRE_NOTHROW( query = queries::GetRichQuery( { { "word", "Городской" } },
          MakeStats( { { "Городской", 0.3 } } ), xx, lp, fm ) ) && REQUIRE( query != nullptr ) )
        {
          SECTION( "entities are found with positions" )
          {
            if ( REQUIRE( query->SearchDoc( 1 ) == 1 ) )
            {
              auto  abstract = query->GetTuples( 1 );

              REQUIRE( abstract.dwMode == abstract.Rich );
              REQUIRE( abstract.nWords == 16 );
              if ( REQUIRE( abstract.entries.size() == 1 ) )
              {
                if ( REQUIRE( abstract.entries.beg->limits.uMin == 10 ) )
                  REQUIRE( abstract.entries.beg->limits.uMax == 10 );
                if ( REQUIRE( abstract.entries.beg->spread.size() == 1 ) )
                  REQUIRE( abstract.entries.beg->spread.pbeg->offset == 10 );
              }
            }
            if ( REQUIRE( query->SearchDoc( 2 ) == 3 ) )
            {
              auto  abstract = query->GetTuples( 3 );

              REQUIRE( abstract.dwMode == abstract.Rich );
              REQUIRE( abstract.nWords == 2 );
              if ( REQUIRE( abstract.entries.size() == 1 ) )
              {
                if ( REQUIRE( abstract.entries.beg->limits.uMin == 0 ) )
                  REQUIRE( abstract.entries.beg->limits.uMax == 0 );
                if ( REQUIRE( abstract.entries.beg->spread.size() == 1 ) )
                  REQUIRE( abstract.entries.beg->spread.pbeg->offset == 0 );
              }
            }
          }
        }
        if ( REQUIRE_NOTHROW( query = queries::GetRichQuery( { { "word", "фонарь" } },
          MakeStats( { { "фонарь", 0.3 } } ), xx, lp, fm ) ) && REQUIRE( query != nullptr ) )
        {
          SECTION( "entities are found with positions" )
          {
            if ( REQUIRE( query->SearchDoc( 1 ) == 1 ) )
            {
              auto  abstract = query->GetTuples( 1 );

              REQUIRE( abstract.dwMode == abstract.Rich );
              REQUIRE( abstract.nWords == 16 );
              if ( REQUIRE( abstract.entries.size() == 2 ) )
              {
                REQUIRE( abstract.entries.beg[0].limits.uMin == 12 );
                REQUIRE( abstract.entries.beg[1].limits.uMin == 15 );
              }
            }
            if ( REQUIRE( query->SearchDoc( 2 ) == 2 ) )
            {
              auto  abstract = query->GetTuples( 2 );

              REQUIRE( abstract.dwMode == abstract.Rich );
              REQUIRE( abstract.nWords == 4 );
              if ( REQUIRE( abstract.entries.size() == 1 ) )
              {
                REQUIRE( abstract.entries.beg->limits.uMin == 2 );
                if ( REQUIRE( abstract.entries.beg->spread.size() == 1 ) )
                  REQUIRE( abstract.entries.beg->spread.pbeg->offset == 2 );
              }
            }
          }
        }
      }
      SECTION( "* && query finds entities with both words" )
      {
        if ( REQUIRE_NOTHROW( query = queries::GetRichQuery( { { "&&", mtc::array_zval{ "городской", "фонарь" } } },
          MakeStats( {
            { "городской", 0.4 },
            { "фонарь",    0.3 } } ), xx, lp, fm ) ) && REQUIRE( query != nullptr ) )
        {
          SECTION( "entities are found with positions" )
          {
            if ( REQUIRE( query->SearchDoc( 1 ) == 1 ) )
            {
              auto  abstract = query->GetTuples( 1 );

              REQUIRE( abstract.dwMode == abstract.Rich );
              REQUIRE( abstract.nWords == 16 );
              if ( REQUIRE( abstract.entries.size() == 1 ) )
              {
                if ( REQUIRE( abstract.entries.beg->limits.uMin == 10 ) )
                  REQUIRE( abstract.entries.beg->limits.uMax == 12 );
                if ( REQUIRE( abstract.entries.beg->spread.size() == 2 ) )
                {
                  REQUIRE( abstract.entries.beg->spread.pbeg[0].offset == 10 );
                  REQUIRE( abstract.entries.beg->spread.pbeg[1].offset == 12 );
                }
              }
            }
          }
        }
      }
      SECTION( "* || query finds entities with any word" )
      {
        if ( REQUIRE_NOTHROW( query = queries::GetRichQuery( { { "||", mtc::array_zval{ "городской", "фонарь" } } },
          MakeStats( {
            { "городской", 0.4 },
            { "фонарь",    0.3 } } ), xx, lp, fm ) ) && REQUIRE( query != nullptr ) )
        {
          SECTION( "entities are found with positions" )
          {
            if ( REQUIRE( query->SearchDoc( 1 ) == 1 ) )
            {
              auto  abstract = query->GetTuples( 1 );

              REQUIRE( abstract.dwMode == abstract.Rich );
              REQUIRE( abstract.nWords == 16 );
              if ( REQUIRE( abstract.entries.size() == 3 ) )
              {
                REQUIRE( abstract.entries.beg[0].limits.uMin == 10 );
                REQUIRE( abstract.entries.beg[1].limits.uMin == 12 );
                REQUIRE( abstract.entries.beg[2].limits.uMin == 15 );
              }
            }
            if ( REQUIRE( query->SearchDoc( 2 ) == 2 ) )
            {
              auto  abstract = query->GetTuples( 2 );

              REQUIRE( abstract.dwMode == abstract.Rich );
              REQUIRE( abstract.nWords == 4 );
              if ( REQUIRE( abstract.entries.size() == 1 ) )
                REQUIRE( abstract.entries.beg->limits.uMin == 2 );
            }
            if ( REQUIRE( query->SearchDoc( 3 ) == 3 ) )
            {
              auto  abstract = query->GetTuples( 3 );

              REQUIRE( abstract.dwMode == abstract.Rich );
              REQUIRE( abstract.nWords == 2 );
              if ( REQUIRE( abstract.entries.size() == 1 ) )
                 REQUIRE( abstract.entries.beg->limits.uMin == 0 );
            }
          }
        }
      }
      SECTION( "* 'quote' query matches exact phrase" )
      {
        if ( REQUIRE_NOTHROW( query = queries::GetRichQuery( { { "quote", mtc::array_zval{ "старый", "фонарь" } } },
          MakeStats( {
            { "старый", 0.3 },
            { "фонарь", 0.4 } } ), xx, lp, fm ) ) && REQUIRE( query != nullptr ) )
        {
          SECTION( "entities are found with positions" )
          {
            if ( REQUIRE( query->SearchDoc( 1 ) == 1 ) )
            {
              auto  abstract = query->GetTuples( 1 );

              REQUIRE( abstract.dwMode == abstract.Rich );
              REQUIRE( abstract.nWords == 16 );
              if ( REQUIRE( abstract.entries.size() == 1 ) )
              {
                REQUIRE( abstract.entries.beg[0].limits.uMin == 14 );
                REQUIRE( abstract.entries.beg[0].limits.uMax == 15 );
              }
            }
            if ( REQUIRE( query->SearchDoc( 2 ) == 2 ) )
            {
              auto  abstract = query->GetTuples( 2 );

              REQUIRE( abstract.dwMode == abstract.None );
            }
          }
        }
      }
    }
  }
} );
