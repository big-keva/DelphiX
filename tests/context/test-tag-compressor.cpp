# include "../../context/pack-format.hpp"
# include "../../context/ranker-tag.hpp"
# include <mtc/test-it-easy.hpp>

using namespace DelphiX;
using RankerTag = context::RankerTag;

TestItEasy::RegisterFunc  test_tag_compressor( []()
{
  TEST_CASE( "context/formats/compressor" )
  {
    auto  serialized = std::vector<char>();
    auto  formatList = std::vector<RankerTag>{
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
      RankerTag tagset[0x20];
      size_t    ncount;

      SECTION( "* to static array" )
      {
        REQUIRE_NOTHROW( ncount = context::formats::Unpack( tagset, serialized.data(), serialized.size() ) );

        if ( REQUIRE( ncount == 6 ) )
        {
          REQUIRE( tagset[0] == RankerTag{ 0, 20, 28 } );
          REQUIRE( tagset[1] == RankerTag{ 1, 22, 26 } );
          REQUIRE( tagset[2] == RankerTag{ 2, 22, 23 } );
          REQUIRE( tagset[3] == RankerTag{ 2, 25, 26 } );
          REQUIRE( tagset[4] == RankerTag{ 1, 27, 28 } );
          REQUIRE( tagset[5] == RankerTag{ 0, 29, 30 } );
        }
      }
      SECTION( "* as dynamic array" )
      {
        auto  decomp = context::formats::Unpack( serialized );

        if ( REQUIRE( decomp.size() == 6 ) )
        {
          REQUIRE( tagset[0] == RankerTag{ 0, 20, 28 } );
          REQUIRE( tagset[1] == RankerTag{ 1, 22, 26 } );
          REQUIRE( tagset[2] == RankerTag{ 2, 22, 23 } );
          REQUIRE( tagset[3] == RankerTag{ 2, 25, 26 } );
          REQUIRE( tagset[4] == RankerTag{ 1, 27, 28 } );
          REQUIRE( tagset[5] == RankerTag{ 0, 29, 30 } );
        }
      }
    }
  }
} );
