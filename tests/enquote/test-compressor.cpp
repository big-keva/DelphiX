# include "../../textAPI/DOM-text.hpp"
# include "../../enquote/compressor.hpp"
# include "../../context/processor.hpp"
# include <mtc/test-it-easy.hpp>

using namespace DelphiX;

class ByteStmOnStr: public mtc::IByteStream
{
  implement_lifetime_stub

public:
  uint32_t  Get(       void*, uint32_t )
  {
    throw std::logic_error( "not implemented @" __FILE__ ":" LINE_STRING );
  }
  uint32_t  Put( const void* p, uint32_t l )
  {
    return output += mtc::charstr( (const char*)p, l ), l;
  }

  ByteStmOnStr( std::string& out ):
    output( out ) {}

  auto  ptr() const -> mtc::IByteStream*
  {
    return (mtc::IByteStream*)this;
  }
protected:
  std::string&  output;
};

TestItEasy::RegisterFunc  test_compressor( []()
{
  TEST_CASE( "enquote/compressor" )
  {
    context::Processor proc;

    SECTION( "array of TextToken may be packed" )
    {
      auto  ucText = textAPI::Document();
      auto  txBody = context::BaseImage<std::allocator<char>>();

      textAPI::Document{
        "Первая строка текста: просто строка",
        "Вторая строка в новом абзаце",
        { "tag-1", {
          "Строка внутри тега",
          { "tag-2", {
            "Строка внутри вложенного тега" } } } },
        "Третья строка." }.CopyUtf16( &ucText );

      proc.WordBreak( txBody, ucText );
      proc.SetMarkup( txBody, ucText );

      auto  as_vec = std::vector<char>();
      auto  as_str = std::string();
      auto  as_stm = std::string();

      SECTION( "* as std::vector<char>" )
      {
        REQUIRE_NOTHROW( as_vec = enquote::PackWords( txBody.GetTokens() ) );
      }
      SECTION( "* as std::string through function<>" )
      {
        REQUIRE_NOTHROW( enquote::PackWords( [&]( const void* p, size_t l )
          {  as_str += std::string( (const char*)p, l );  }, txBody.GetTokens() ) );
      }
      SECTION( "* as IByteStream" )
      {
        REQUIRE_NOTHROW( enquote::PackWords( ByteStmOnStr( as_stm ).ptr(), txBody.GetTokens() ) );
      }
      SECTION( "and the results are the same" )
      {
        REQUIRE( as_str == as_stm );
        REQUIRE( as_stm.length() == as_vec.size() );
        REQUIRE( memcmp( as_stm.data(), as_vec.data(), as_stm.size() ) == 0 );
      }

      SECTION( "the packed words may be unpacked" )
      {
        auto  unpack = context::Image();

        if ( REQUIRE_NOTHROW( unpack = enquote::UnpackWords( as_vec ) ) )
          REQUIRE( unpack.GetTokens() == txBody.GetTokens() );
      }
    }
  }

} );
