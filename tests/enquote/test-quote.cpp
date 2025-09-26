#include <queries.hpp>

# include "../../textAPI/DOM-text.hpp"
# include "../../enquote/compressor.hpp"
# include "../../context/processor.hpp"
# include "../../context/formats.hpp"
# include <mtc/test-it-easy.hpp>
#include <src/service/collect.hpp>

using namespace DelphiX;

auto  GetPackedImage( const textAPI::Document& doc, FieldHandler& fds ) -> std::pair<std::vector<char>, std::vector<char>>
{
  auto  ucText = textAPI::Document();
  auto  txBody = context::BaseImage<std::allocator<char>>();
  auto  lgProc = context::Processor();

  CopyUtf16( &ucText, doc );

  lgProc.WordBreak( txBody, ucText );
  lgProc.SetMarkup( txBody, ucText );

  return { enquote::PackWords( txBody.GetTokens() ), context::formats::Pack( ucText.GetMarkup(), fds ) };
}

auto  GetUnpackImage( const std::pair<std::vector<char>, std::vector<char>>& image, const FieldHandler& fdhan ) -> context::Image
{
  auto  ximage = context::Image();
  auto  addTag = [&]( const context::formats::FormatTag<unsigned>& tag )
    {
      auto  pf = fdhan.Get( tag.format );

      if ( pf != nullptr )
        ximage.GetMarkup().push_back( { pf->name.data(), tag.uLower, tag.uUpper } );
    };

  enquote::UnpackWords( ximage, image.first );
  context::formats::Unpack( addTag, image.second.data(), image.second.size() );

  return ximage;
}

using Abstract = DelphiX::queries::IQuery::Abstract;
using ITextView = palmira::collect::ITextView;

auto  GetQuotes( const context::Image& image, const Abstract& quote, const FieldHandler& fdhan ) -> mtc::api<ITextView>
{
  auto  ft = image.GetMarkup().begin();
  auto  id = 0;

}

class FdMan: public FieldHandler
{
  FieldOptions  tag1 = { 1, "tag-1" };
  FieldOptions  tag2 = { 2, "tag-2" };

public:
  auto  Add( const StrView& tag ) -> FieldOptions* override
  {
    return
      tag == tag1.name ? &tag1 :
      tag == tag2.name ? &tag2 : nullptr;
  }
  auto  Get( const StrView& tag ) const -> const FieldOptions* override
  {
    return
      tag == tag1.name ? &tag1 :
      tag == tag2.name ? &tag2 : nullptr;
  }
  auto  Get( unsigned id ) const -> const FieldOptions* override
  {
    return
      id == tag1.id ? &tag1 :
      id == tag2.id ? &tag2 : nullptr;
  }
};

TestItEasy::RegisterFunc  test_quote( []()
{
  TEST_CASE( "enquote/quote" )
  {
    FdMan  fdman;
    auto   compr = GetPackedImage( {
      "Первая строка текста: просто строка",
      "Вторая строка в новом абзаце",
      { "tag-1", {
        "Строка внутри тега",
        { "tag-2", {
          "Строка внутри вложенного тега" } } } },
      "Третья строка." }, fdman );

    SECTION( "image text may be reconstructed" )
    {
    }
    SECTION( "image text may be quoted" )
    {
      auto  ximage = GetUnpackImage( compr, fdman );

    }
  }

} );
