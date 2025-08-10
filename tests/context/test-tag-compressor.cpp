# include "../../src/context/serial-formats.hpp"
# include "../../text-api.hpp"
# include <mtc/test-it-easy.hpp>

TestItEasy::RegisterFunc  test_tag_compressor( []()
{
  TEST_CASE( "context/tag-compressor" )
  {
    auto  compressor = DelphiX::context::formats::Compressor();
    auto  serialized = std::vector<char>();

    SECTION( "TagCompressor is filled sequentally" )
    {
      REQUIRE_NOTHROW( compressor.AddMarkup( { 0, 20, 28 } ) );
      REQUIRE_NOTHROW( compressor.AddMarkup( { 1, 22, 25 } ) );
      REQUIRE_NOTHROW( compressor.AddMarkup( { 2, 22, 23 } ) );
      REQUIRE_EXCEPTION( compressor.AddMarkup( { 3, 23, 24 } ), std::logic_error );
      REQUIRE_NOTHROW( compressor.AddMarkup( { 2, 24, 25 } ) );
      REQUIRE_NOTHROW( compressor.AddMarkup( { 1, 26, 27 } ) );
      REQUIRE_NOTHROW( compressor.AddMarkup( { 0, 29, 30 } ) );
    }
    SECTION( "it may be serialized" )
    {
      REQUIRE_NOTHROW( serialized.resize( compressor.GetBufLen() ) );
      REQUIRE( compressor.Serialize( serialized.data() ) - serialized.data() == serialized.size() );
    }
    SECTION( "decompressor works with serialized data" )
    {
      SECTION( "it may be iterated element by element" )
      {
        auto  decomp = DelphiX::context::formats::Decompressor( serialized.data() );

        if ( REQUIRE( decomp.GetTag() != nullptr ) )
          REQUIRE( *decomp.GetTag() == DelphiX::textAPI::FormatTag<unsigned>{ 0, 20, 28 } );
        if ( REQUIRE( decomp.ToNext() != nullptr ) )
          REQUIRE( *decomp.GetTag() == DelphiX::textAPI::FormatTag<unsigned>{ 1, 22, 25 } );
        if ( REQUIRE( decomp.ToNext() != nullptr ) )
          REQUIRE( *decomp.GetTag() == DelphiX::textAPI::FormatTag<unsigned>{ 2, 22, 23 } );
        if ( REQUIRE( decomp.ToNext() != nullptr ) )
          REQUIRE( *decomp.GetTag() == DelphiX::textAPI::FormatTag<unsigned>{ 2, 24, 25 } );
        if ( REQUIRE( decomp.ToNext() != nullptr ) )
          REQUIRE( *decomp.GetTag() == DelphiX::textAPI::FormatTag<unsigned>{ 1, 26, 27 } );
        if ( REQUIRE( decomp.ToNext() != nullptr ) )
          REQUIRE( *decomp.GetTag() == DelphiX::textAPI::FormatTag<unsigned>{ 0, 29, 30 } );
        REQUIRE( decomp.ToNext() == nullptr );
      }
      SECTION( "elements may be searched for desired format" )
      {
        auto  decomp = DelphiX::context::formats::Decompressor( serialized.data() );

        if ( REQUIRE( decomp.ToNext( 2 ) != nullptr ) )
          REQUIRE( *decomp.GetTag() == DelphiX::textAPI::FormatTag<unsigned>{ 2, 22, 23 } );
        if ( REQUIRE( decomp.ToNext( 2 ) != nullptr ) )
          REQUIRE( *decomp.GetTag() == DelphiX::textAPI::FormatTag<unsigned>{ 2, 24, 25 } );
        REQUIRE( decomp.ToNext( 2 ) == nullptr );
      }
    }
  }
} );
