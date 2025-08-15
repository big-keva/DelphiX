# include "../../src/context/formats.hpp"
# include "../../text-api.hpp"
# include <mtc/test-it-easy.hpp>

using namespace DelphiX;

TestItEasy::RegisterFunc  test_tag_compressor( []()
{
  TEST_CASE( "context/tag-compressor" )
  {
    auto  compressor = DelphiX::context::formats::Compressor();
    auto  serialized = std::vector<char>();

    SECTION( "TagCompressor is filled sequentally" )
    {
      REQUIRE_NOTHROW( compressor.AddMarkup( { 0, 20, 28 } ) );
      REQUIRE_NOTHROW( compressor.AddMarkup( { 1, 22, 26 } ) );
      REQUIRE_NOTHROW( compressor.AddMarkup( { 2, 22, 23 } ) );
      REQUIRE_EXCEPTION( compressor.AddMarkup( { 3, 23, 24 } ), std::logic_error );
      REQUIRE_NOTHROW( compressor.AddMarkup( { 2, 25, 26 } ) );
      REQUIRE_NOTHROW( compressor.AddMarkup( { 1, 27, 28 } ) );
      REQUIRE_NOTHROW( compressor.AddMarkup( { 0, 29, 30 } ) );
    }
    SECTION( "it may be serialized..." )
    {
      REQUIRE_NOTHROW( serialized.resize( compressor.GetBufLen() ) );
      REQUIRE( compressor.Serialize( serialized.data() ) - serialized.data() == serialized.size() );
    }
    SECTION( "... and deserialized" )
    {
      textAPI::FormatTag<unsigned>  tagset[0x20];
      unsigned                      ncount;

      SECTION( "* to static array" )
      {
        REQUIRE_NOTHROW( ncount = context::formats::Unpack( tagset, serialized.data(), serialized.size() ) );
        REQUIRE( ncount == 6 );
        REQUIRE( tagset[0] == textAPI::FormatTag<unsigned>{ 0, 20, 28 } );
        REQUIRE( tagset[1] == textAPI::FormatTag<unsigned>{ 1, 22, 26 } );
        REQUIRE( tagset[2] == textAPI::FormatTag<unsigned>{ 2, 22, 23 } );
        REQUIRE( tagset[3] == textAPI::FormatTag<unsigned>{ 2, 25, 26 } );
        REQUIRE( tagset[4] == textAPI::FormatTag<unsigned>{ 1, 27, 28 } );
        REQUIRE( tagset[5] == textAPI::FormatTag<unsigned>{ 0, 29, 30 } );
      }
      SECTION( "it may be iterated element by element" )
      {
/*        auto  decomp = DelphiX::context::formats::Decompressor( serialized.data() );

        if ( REQUIRE( decomp.GetCurr() != nullptr ) )
          REQUIRE( *decomp.GetCurr() == DelphiX::textAPI::FormatTag<unsigned>{ 0, 20, 28 } );
        if ( REQUIRE( decomp.GetNext() != nullptr ) )
          REQUIRE( *decomp.GetCurr() == DelphiX::textAPI::FormatTag<unsigned>{ 1, 22, 26 } );
        if ( REQUIRE( decomp.GetNext() != nullptr ) )
          REQUIRE( *decomp.GetCurr() == DelphiX::textAPI::FormatTag<unsigned>{ 2, 22, 23 } );
        if ( REQUIRE( decomp.GetNext() != nullptr ) )
          REQUIRE( *decomp.GetCurr() == DelphiX::textAPI::FormatTag<unsigned>{ 2, 25, 26 } );
        if ( REQUIRE( decomp.GetNext() != nullptr ) )
          REQUIRE( *decomp.GetCurr() == DelphiX::textAPI::FormatTag<unsigned>{ 1, 27, 28 } );
        if ( REQUIRE( decomp.GetNext() != nullptr ) )
          REQUIRE( *decomp.GetCurr() == DelphiX::textAPI::FormatTag<unsigned>{ 0, 29, 30 } );
        REQUIRE( decomp.GetNext() == nullptr );*/
      }
      SECTION( "elements may be listed for desired format" )
      {
/*        auto  decomp = DelphiX::context::formats::Decompressor( serialized.data() );

        if ( REQUIRE( decomp.GetNext( 2 ) != nullptr ) )
          REQUIRE( *decomp.GetCurr() == DelphiX::textAPI::FormatTag<unsigned>{ 2, 22, 23 } );
        if ( REQUIRE( decomp.GetNext( 2 ) != nullptr ) )
          REQUIRE( *decomp.GetCurr() == DelphiX::textAPI::FormatTag<unsigned>{ 2, 25, 26 } );
        REQUIRE( decomp.GetNext( 2 ) == nullptr );*/
      }
      SECTION( "elements may be searched for deepest format" )
      {
/*        auto  decomp = DelphiX::context::formats::Decompressor( serialized.data() );
        auto  format = decltype(decomp.FindTag( 20 )){};

        REQUIRE( decomp.FindTag( 2 ) == nullptr );

        if ( REQUIRE( (format = decomp.FindTag( 20 )) != nullptr ) )
          REQUIRE( *format == DelphiX::textAPI::FormatTag<unsigned>{ 0, 20, 28 } );
        if ( REQUIRE( (format = decomp.FindTag( 21 )) != nullptr ) )
          REQUIRE( *format == DelphiX::textAPI::FormatTag<unsigned>{ 0, 20, 28 } );
        if ( REQUIRE( (format = decomp.FindTag( 22 )) != nullptr ) )
          REQUIRE( *format == DelphiX::textAPI::FormatTag<unsigned>{ 2, 22, 23 } );
        if ( REQUIRE( (format = decomp.FindTag( 24 )) != nullptr ) )
          REQUIRE( *format == DelphiX::textAPI::FormatTag<unsigned>{ 1, 22, 26 } );
        if ( REQUIRE( (format = decomp.FindTag( 27 )) != nullptr ) )
          REQUIRE( *format == DelphiX::textAPI::FormatTag<unsigned>{ 1, 27, 28 } );
        if ( REQUIRE( (format = decomp.FindTag( 31 )) != nullptr ) )
          REQUIRE( *format == DelphiX::textAPI::FormatTag<unsigned>{ 0, 30, 31 } );

        REQUIRE( decomp.FindTag( 32 ) == nullptr );*/
      }
    }
  }
} );
