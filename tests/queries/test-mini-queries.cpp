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

auto  CreateMiniIndex( const context::Processor& lp, const std::initializer_list<Document>& docs ) -> mtc::api<IContentsIndex>
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
    auto  ximage = GetMiniContents( ucBody.GetLemmas(), ucBody.GetMarkup(), fieldMan );
    ct->SetEntity( mtc::strprintf( "doc-%u", id++ ), ximage.ptr() );
  }
  return ct;
}

TestItEasy::RegisterFunc  test_mini_queries( []()
{
  TEST_CASE( "queries/rich" )
  {
    auto  lp = context::Processor();
    auto  xx = CreateMiniIndex( lp, {
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

    SECTION( "mini query may be created from it's structured representation" )
    {
      mtc::api<queries::IQuery> query;

      SECTION( "* invalid query causes std::invalid_argument" )
      {
        REQUIRE_EXCEPTION( query = queries::BuildMiniQuery( {}, {}, xx, lp, fieldMan ), std::invalid_argument );
      }
      SECTION( "* for non-collection term the query is empty" )
      {
        if ( REQUIRE_NOTHROW( query = queries::BuildMiniQuery( "Платон", {}, xx, lp, fieldMan ) ) )
          REQUIRE( query == nullptr );
      }
      SECTION( "* existing term produces a query" )
      {
        if ( REQUIRE_NOTHROW( query = queries::BuildMiniQuery( "Городской", {}, xx, lp, fieldMan ) )
          && REQUIRE( query != nullptr ) )
        {
          SECTION( "entities are found with positions" )
          {
            if ( REQUIRE( query->SearchDoc( 1 ) == 1 ) )
            {
              auto  abstract = query->GetTuples( 1 );

              REQUIRE( abstract.dwMode == abstract.BM25 );
              REQUIRE( abstract.factors.size() == 1 );
            }
            if ( REQUIRE( query->SearchDoc( 2 ) == 3 ) )
            {
              auto  abstract = query->GetTuples( 3 );

              REQUIRE( abstract.dwMode == abstract.BM25 );
              REQUIRE( abstract.factors.size() == 1 );
            }
          }
        }
        if ( REQUIRE_NOTHROW( query = queries::BuildMiniQuery( "фонарь", {}, xx, lp, fieldMan ) )
          && REQUIRE( query != nullptr ) )
        {
          SECTION( "entities are found with positions" )
          {
            if ( REQUIRE( query->SearchDoc( 1 ) == 1 ) )
            {
              auto  abstract = query->GetTuples( 1 );

              REQUIRE( abstract.dwMode == abstract.BM25 );
            }
            if ( REQUIRE( query->SearchDoc( 2 ) == 2 ) )
            {
              auto  abstract = query->GetTuples( 2 );

              REQUIRE( abstract.dwMode == abstract.BM25 );
            }
          }
        }
      }
      SECTION( "* wildcard produces a complex query" )
      {
        if ( REQUIRE_NOTHROW( query = queries::BuildMiniQuery( mtc::zmap{
          { "wildcard", "с*й" } }, {}, xx, lp, fieldMan ) ) && REQUIRE( query != nullptr ) )
        {
          SECTION( "entities are found with positions" )
          {
            if ( REQUIRE( query->SearchDoc( 1 ) == 1 ) )
              REQUIRE( query->GetTuples( 1 ).factors.size() == 1 );
            if ( REQUIRE( query->SearchDoc( 2 ) == 2 ) )
              REQUIRE( query->GetTuples( 2 ).factors.size() == 1 );
            if ( REQUIRE( query->SearchDoc( 3 ) == 3 ) )
              REQUIRE( query->GetTuples( 3 ).factors.size() == 1 );
          }
        }
        if ( REQUIRE_NOTHROW( query = queries::BuildMiniQuery( "фонарь", {}, xx, lp, fieldMan ) )
          && REQUIRE( query != nullptr ) )
        {
          SECTION( "entities are found with positions" )
          {
            if ( REQUIRE( query->SearchDoc( 1 ) == 1 ) )
            {
              auto  abstract = query->GetTuples( 1 );

              REQUIRE( abstract.dwMode == abstract.BM25 );
            }
            if ( REQUIRE( query->SearchDoc( 2 ) == 2 ) )
            {
              auto  abstract = query->GetTuples( 2 );

              REQUIRE( abstract.dwMode == abstract.BM25 );
            }
          }
        }
      }
      SECTION( "* && query finds entities with both words" )
      {
        if ( REQUIRE_NOTHROW( query = queries::BuildMiniQuery( mtc::zmap{ { "&&", mtc::array_zval{ "городской", "фонарь" } } }, {}, xx, lp, fieldMan ) )
          && REQUIRE( query != nullptr ) )
        {
          SECTION( "entities are found with positions" )
          {
            if ( REQUIRE( query->SearchDoc( 1 ) == 1 ) )
            {
              auto  abstract = query->GetTuples( 1 );

              REQUIRE( abstract.dwMode == abstract.BM25 );
              REQUIRE( abstract.factors.size() == 2 );
            }
          }
        }
      }
      SECTION( "* || query finds entities with any word" )
      {
        if ( REQUIRE_NOTHROW( query = queries::BuildMiniQuery( mtc::zmap{ { "||", mtc::array_zval{ "городской", "фонарь" } } }, {}, xx, lp, fieldMan ) )
          && REQUIRE( query != nullptr ) )
        {
          SECTION( "entities are found with positions" )
          {
            if ( REQUIRE( query->SearchDoc( 1 ) == 1 ) )
            {
              auto  abstract = query->GetTuples( 1 );

              REQUIRE( abstract.dwMode == abstract.BM25 );
              REQUIRE( abstract.factors.size() == 1 );
            }
            if ( REQUIRE( query->SearchDoc( 2 ) == 2 ) )
            {
              auto  abstract = query->GetTuples( 2 );

              REQUIRE( abstract.dwMode == abstract.BM25 );
              REQUIRE( abstract.factors.size() == 1 );
            }
            if ( REQUIRE( query->SearchDoc( 3 ) == 3 ) )
            {
              auto  abstract = query->GetTuples( 3 );

              REQUIRE( abstract.dwMode == abstract.BM25 );
              REQUIRE( abstract.factors.size() == 1 );
            }
          }
        }
      }
      SECTION( "* 'quote' query matches exact phrase" )
      {
        if ( REQUIRE_NOTHROW( query = queries::BuildMiniQuery( mtc::zmap{ { "quote", mtc::array_zval{ "старый", "фонарь" } } }, {}, xx, lp, fieldMan ) )
          && REQUIRE( query != nullptr ) )
        {
          SECTION( "entities are found with positions" )
          {
            if ( REQUIRE( query->SearchDoc( 1 ) == 1 ) )
            {
              auto  abstract = query->GetTuples( 1 );

              REQUIRE( abstract.dwMode == abstract.BM25 );
              REQUIRE( abstract.factors.size() == 2 );
            }
            if ( REQUIRE( query->SearchDoc( 2 ) == 2 ) )
            {
              auto  abstract = query->GetTuples( 2 );

              REQUIRE( abstract.dwMode == abstract.BM25 );
              REQUIRE( abstract.factors.size() == 2 );
            }
          }
        }
      }
      SECTION( "* fuzzy query searches best entries using query quorum" )
      {
        if ( REQUIRE_NOTHROW( query = queries::BuildMiniQuery( mtc::zmap{ { "fuzzy", mtc::array_zval{ "городской", "фонарь" } } },
            MakeStats( { { "городской", 0.7 }, { "фонарь", 0.05 } } ), xx, lp, fieldMan ) )
          && REQUIRE( query != nullptr ) )
        {
          SECTION( "entities are found with positions" )
          {
            if ( REQUIRE( query->SearchDoc( 1 ) == 1 ) )
            {
              auto  abstract = query->GetTuples( 1 );

              REQUIRE( abstract.dwMode == abstract.BM25 );
              REQUIRE( abstract.factors.size() == 2 );
            }
            if ( REQUIRE( query->SearchDoc( 2 ) == 3 ) )
            {
              auto  abstract = query->GetTuples( 3 );

              REQUIRE( abstract.dwMode == abstract.BM25 );
              REQUIRE( abstract.factors.size() == 1 );
            }
          }
        }
      }
    }
  }
} );
