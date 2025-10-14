# include "../../context/x-contents.hpp"
# include "../../context/processor.hpp"
# include "../../queries/builder.hpp"
# include "../../src/queries/field-set.hpp"
# include "../../indexer/dynamic-contents.hpp"
# include "../../textAPI/DOM-dump.hpp"
# include <mtc/test-it-easy.hpp>

using namespace DelphiX;
using namespace DelphiX::textAPI;

static  auto  _W( const char* s ) -> mtc::widestr
  {  return codepages::mbcstowide( codepages::codepage_utf8, s );  }

static  auto  MakeStats( const std::initializer_list<std::pair<const char*, double>>& init ) -> mtc::zmap
{
  auto  ts = mtc::zmap();

  for ( auto& term: init )
    ts.set_zmap( _W( term.first ), { { "range", term.second } } );
  return { { "terms-range-map", ts } };
}

static  context::FieldManager fieldMan;

auto  CreateRichIndex( const context::Processor& lp, const std::initializer_list<Document>& docs ) -> mtc::api<IContentsIndex>
{
  auto  ct = indexer::dynamic::Index().Create();
  auto  id = 0;

  for ( auto& doc: docs )
  {
    auto  ucText = Document();
      CopyUtf16( &ucText, doc );
    auto  ucBody = context::Image();

    lp.SetMarkup(
    lp.Lemmatize(
    lp.WordBreak( ucBody, ucText ) ), ucText );

//    ucBody.Serialize( dump_as::Json( dump_as::MakeOutput( stdout ) ) );
    ct->SetEntity( mtc::strprintf( "doc-%u", id++ ),
      GetRichContents( ucBody.GetLemmas(), ucBody.GetMarkup(), fieldMan ).ptr() );
  }
  return ct;
}

TestItEasy::RegisterFunc  test_rich_queries( []()
{
  TEST_CASE( "queries/rich" )
  {
    auto  lp = context::Processor();
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

    SECTION( "field sets arythmetics intersect and join fields" )
    {
      auto  f1 = queries::FieldSet{ 1, 3, 2, 5, 4 };
      auto  f2 = queries::FieldSet{ 5, 2, 7, 6, 8 };

      SECTION( "it may be intersected" )
      {
        REQUIRE( (f1 & f2) == queries::FieldSet{ 2, 5 } );
      }
      SECTION( "it may be joined" )
      {
        REQUIRE( (f1 | f2) == queries::FieldSet{ 1, 2, 3, 4, 5, 6, 7, 8 } );
      }
    }
    SECTION( "rich query may be created from it's structured representation" )
    {
      mtc::api<queries::IQuery> query;

      SECTION( "* invalid query causes std::invalid_argument" )
      {
        REQUIRE_EXCEPTION( query = queries::BuildRichQuery( {}, {}, xx, lp, fieldMan ), std::invalid_argument );
      }
      SECTION( "* for non-collection term the query is empty" )
      {
        if ( REQUIRE_NOTHROW( query = queries::BuildRichQuery( "Платон", {}, xx, lp, fieldMan ) ) )
          REQUIRE( query == nullptr );
      }
      SECTION( "* existing term produces a query" )
      {
        if ( REQUIRE_NOTHROW( query = queries::BuildRichQuery( "Городской", {}, xx, lp, fieldMan ) )
          && REQUIRE( query != nullptr ) )
        {
          SECTION( "entities are found with positions" )
          {
            if ( REQUIRE( query->SearchDoc( 1 ) == 1 ) )
            {
              auto  abstract = query->GetTuples( 1 );

              REQUIRE( abstract.dwMode == abstract.Rich );
              REQUIRE( abstract.nWords == 16 );
              if ( REQUIRE( abstract.entries.size() == 1U ) )
              {
                if ( REQUIRE( abstract.entries.pbeg->limits.uMin == 10 ) )
                  REQUIRE( abstract.entries.pbeg->limits.uMax == 10 );
                if ( REQUIRE( abstract.entries.pbeg->spread.size() == 1U ) )
                  REQUIRE( abstract.entries.pbeg->spread.pbeg->offset == 10 );
              }
            }
            if ( REQUIRE( query->SearchDoc( 2 ) == 3 ) )
            {
              auto  abstract = query->GetTuples( 3 );

              REQUIRE( abstract.dwMode == abstract.Rich );
              REQUIRE( abstract.nWords == 2 );
              if ( REQUIRE( abstract.entries.size() == 1U ) )
              {
                if ( REQUIRE( abstract.entries.pbeg->limits.uMin == 0 ) )
                  REQUIRE( abstract.entries.pbeg->limits.uMax == 0 );
                if ( REQUIRE( abstract.entries.pbeg->spread.size() == 1U ) )
                  REQUIRE( abstract.entries.pbeg->spread.pbeg->offset == 0 );
              }
            }
          }
        }
        if ( REQUIRE_NOTHROW( query = queries::BuildRichQuery( "фонарь", {}, xx, lp, fieldMan ) )
          && REQUIRE( query != nullptr ) )
        {
          SECTION( "entities are found with positions" )
          {
            if ( REQUIRE( query->SearchDoc( 1 ) == 1 ) )
            {
              auto  abstract = query->GetTuples( 1 );

              REQUIRE( abstract.dwMode == abstract.Rich );
              REQUIRE( abstract.nWords == 16 );
              if ( REQUIRE( abstract.entries.size() == 2U ) )
              {
                REQUIRE( abstract.entries.pbeg[0].limits.uMin == 12 );
                REQUIRE( abstract.entries.pbeg[1].limits.uMin == 15 );
              }
            }
            if ( REQUIRE( query->SearchDoc( 2 ) == 2 ) )
            {
              auto  abstract = query->GetTuples( 2 );

              REQUIRE( abstract.dwMode == abstract.Rich );
              REQUIRE( abstract.nWords == 4 );
              if ( REQUIRE( abstract.entries.size() == 1U ) )
              {
                REQUIRE( abstract.entries.pbeg->limits.uMin == 2 );
                if ( REQUIRE( abstract.entries.pbeg->spread.size() == 1U ) )
                  REQUIRE( abstract.entries.pbeg->spread.pbeg->offset == 2 );
              }
            }
          }
        }
      }
      SECTION( "* && query finds entities with both words" )
      {
        if ( REQUIRE_NOTHROW( query = queries::BuildRichQuery( mtc::zmap{ { "&&", mtc::array_zval{ "городской", "фонарь" } } }, {}, xx, lp, fieldMan ) )
          && REQUIRE( query != nullptr ) )
        {
          SECTION( "entities are found with positions" )
          {
            if ( REQUIRE( query->SearchDoc( 1 ) == 1 ) )
            {
              auto  abstract = query->GetTuples( 1 );

              REQUIRE( abstract.dwMode == abstract.Rich );
              REQUIRE( abstract.nWords == 16 );
              if ( REQUIRE( abstract.entries.size() == 1U ) )
              {
                if ( REQUIRE( abstract.entries.pbeg->limits.uMin == 10 ) )
                  REQUIRE( abstract.entries.pbeg->limits.uMax == 12 );
                if ( REQUIRE( abstract.entries.pbeg->spread.size() == 2U ) )
                {
                  REQUIRE( abstract.entries.pbeg->spread.pbeg[0].offset == 10 );
                  REQUIRE( abstract.entries.pbeg->spread.pbeg[1].offset == 12 );
                }
              }
            }
          }
        }
      }
      SECTION( "* || query finds entities with any word" )
      {
        if ( REQUIRE_NOTHROW( query = queries::BuildRichQuery( mtc::zmap{ { "||", mtc::array_zval{ "городской", "фонарь" } } }, {}, xx, lp, fieldMan ) )
          && REQUIRE( query != nullptr ) )
        {
          SECTION( "entities are found with positions" )
          {
            if ( REQUIRE( query->SearchDoc( 1 ) == 1 ) )
            {
              auto  abstract = query->GetTuples( 1 );

              REQUIRE( abstract.dwMode == abstract.Rich );
              REQUIRE( abstract.nWords == 16 );
              if ( REQUIRE( abstract.entries.size() == 3U ) )
              {
                REQUIRE( abstract.entries.pbeg[0].limits.uMin == 10 );
                REQUIRE( abstract.entries.pbeg[1].limits.uMin == 12 );
                REQUIRE( abstract.entries.pbeg[2].limits.uMin == 15 );
              }
            }
            if ( REQUIRE( query->SearchDoc( 2 ) == 2 ) )
            {
              auto  abstract = query->GetTuples( 2 );

              REQUIRE( abstract.dwMode == abstract.Rich );
              REQUIRE( abstract.nWords == 4 );
              if ( REQUIRE( abstract.entries.size() == 1U ) )
                REQUIRE( abstract.entries.pbeg->limits.uMin == 2 );
            }
            if ( REQUIRE( query->SearchDoc( 3 ) == 3 ) )
            {
              auto  abstract = query->GetTuples( 3 );

              REQUIRE( abstract.dwMode == abstract.Rich );
              REQUIRE( abstract.nWords == 2 );
              if ( REQUIRE( abstract.entries.size() == 1U ) )
                 REQUIRE( abstract.entries.pbeg->limits.uMin == 0 );
            }
          }
        }
      }
      SECTION( "* 'quote' query matches exact phrase" )
      {
        if ( REQUIRE_NOTHROW( query = queries::BuildRichQuery( mtc::zmap{ { "quote", mtc::array_zval{ "старый", "фонарь" } } }, {}, xx, lp, fieldMan ) )
          && REQUIRE( query != nullptr ) )
        {
          SECTION( "entities are found with positions" )
          {
            if ( REQUIRE( query->SearchDoc( 1 ) == 1 ) )
            {
              auto  abstract = query->GetTuples( 1 );

              REQUIRE( abstract.dwMode == abstract.Rich );
              REQUIRE( abstract.nWords == 16 );
              if ( REQUIRE( abstract.entries.size() == 1U ) )
              {
                REQUIRE( abstract.entries.pbeg[0].limits.uMin == 14 );
                REQUIRE( abstract.entries.pbeg[0].limits.uMax == 15 );
                REQUIRE( abstract.entries.pbeg[0].spread.size() == 2U );
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
      SECTION( "* fuzzy query searches best entries using query quorum" )
      {
        if ( REQUIRE_NOTHROW( query = queries::BuildRichQuery( mtc::zmap{ { "fuzzy", mtc::array_zval{ "городской", "фонарь" } } },
            MakeStats( { { "городской", 0.7 }, { "фонарь", 0.05 } } ), xx, lp, fieldMan ) )
          && REQUIRE( query != nullptr ) )
        {
          SECTION( "entities are found with positions" )
          {
            if ( REQUIRE( query->SearchDoc( 1 ) == 1 ) )
            {
              auto  abstract = query->GetTuples( 1 );

              REQUIRE( abstract.dwMode == abstract.Rich );
              REQUIRE( abstract.nWords == 16 );
              if ( REQUIRE( abstract.entries.size() == 1U ) )
              {
                REQUIRE( abstract.entries.pbeg[0].limits.uMin == 10 );
                REQUIRE( abstract.entries.pbeg[0].limits.uMax == 12 );
                REQUIRE( abstract.entries.pbeg[0].spread.size() == 2U );
              }
            }
            if ( REQUIRE( query->SearchDoc( 2 ) == 3 ) )
            {
              auto  abstract = query->GetTuples( 3 );

              REQUIRE( abstract.dwMode == abstract.Rich );
              REQUIRE( abstract.nWords == 2 );
              if ( REQUIRE( abstract.entries.size() == 1U ) )
              {
                REQUIRE( abstract.entries.pbeg[0].limits.uMin == 0 );
                REQUIRE( abstract.entries.pbeg[0].limits.uMax == 0 );
                REQUIRE( abstract.entries.pbeg[0].spread.size() == 1U );
              }
            }
          }
        }
      }
    }
  }
} );
