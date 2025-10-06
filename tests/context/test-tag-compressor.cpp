# include "../../context/pack-format.hpp"
# include "../../text-api.hpp"
# include <mtc/test-it-easy.hpp>

using namespace DelphiX;

TestItEasy::RegisterFunc  test_tag_compressor( []()
{
  TEST_CASE( "context/formats/compressor" )
  {
    auto  serialized = std::vector<char>();
    auto  formatList = std::vector<textAPI::FormatTag<unsigned>>{
      { 0, 20, 28 },
      { 1, 22, 26 },
      { 2, 22, 23 },
      { 2, 25, 26 },
      { 1, 27, 28 },
      { 0, 29, 30 } };

    SECTION( "it may be serialized..." )
    {
      REQUIRE_NOTHROW( serialized = context::formats::Pack( formatList ) );
    }
    SECTION( "... and deserialized" )
    {
      textAPI::FormatTag<unsigned>  tagset[0x20];
      unsigned                      ncount;

      SECTION( "* to static array" )
      {
        REQUIRE_NOTHROW( ncount = context::formats::Unpack( tagset, serialized.data(), serialized.size() ) );

        if ( REQUIRE( ncount == 6 ) )
        {
          REQUIRE( tagset[0] == textAPI::FormatTag<unsigned>{ 0, 20, 28 } );
          REQUIRE( tagset[1] == textAPI::FormatTag<unsigned>{ 1, 22, 26 } );
          REQUIRE( tagset[2] == textAPI::FormatTag<unsigned>{ 2, 22, 23 } );
          REQUIRE( tagset[3] == textAPI::FormatTag<unsigned>{ 2, 25, 26 } );
          REQUIRE( tagset[4] == textAPI::FormatTag<unsigned>{ 1, 27, 28 } );
          REQUIRE( tagset[5] == textAPI::FormatTag<unsigned>{ 0, 29, 30 } );
        }
      }
      SECTION( "* as dynamic array" )
      {
        auto  decomp = context::formats::Unpack( serialized );

        if ( REQUIRE( decomp.size() == 6 ) )
        {
          REQUIRE( tagset[0] == textAPI::FormatTag<unsigned>{ 0, 20, 28 } );
          REQUIRE( tagset[1] == textAPI::FormatTag<unsigned>{ 1, 22, 26 } );
          REQUIRE( tagset[2] == textAPI::FormatTag<unsigned>{ 2, 22, 23 } );
          REQUIRE( tagset[3] == textAPI::FormatTag<unsigned>{ 2, 25, 26 } );
          REQUIRE( tagset[4] == textAPI::FormatTag<unsigned>{ 1, 27, 28 } );
          REQUIRE( tagset[5] == textAPI::FormatTag<unsigned>{ 0, 29, 30 } );
        }
      }
    }
  }
} );
