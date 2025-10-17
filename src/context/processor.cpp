# include "../../context/processor.hpp"

namespace DelphiX {
namespace context {

  auto  Processor::WordBreak( const textAPI::ITextView& input, const FieldHandler* fdset ) const -> Image
  {
    Image   image;

    return std::move( WordBreak( image, input, fdset ) );
  }

  auto  Processor::MakeImage( const textAPI::ITextView& input, const FieldHandler* fdset ) const -> Image
  {
    Image   image;

    return std::move( MakeImage( image, input, fdset ) );
  }

  auto  Processor::Lemmatize( const mtc::widestr& str ) const -> std::vector<Lexeme>
  {
    std::vector<Lexeme> lexbuf;

    return std::move( Lemmatize( lexbuf, str.c_str(), str.length() ) );
  }

  auto  Processor::Initialize( unsigned langId, const mtc::api<ILemmatizer>& module ) -> Processor&
  {
    languages.push_back( { langId, module } );
    return *this;
  }

  auto  Processor::Initialize( const Slice<const std::pair<unsigned, const mtc::api<ILemmatizer>>>& init ) ->Processor&
  {
    for ( auto& next: init )
      languages.push_back( { next.first, next.second } );
    return *this;
  }

  void  Processor::MapMarkup(
    const Slice<textAPI::MarkupTag>&        markup,
    const Slice<const textAPI::TextToken>&  tokens ) const
  {
    auto  fwdFmt = std::vector<textAPI::MarkupTag*>();
    auto  bckFmt = std::vector<textAPI::MarkupTag*>();

    // create formats indices
    for ( auto beg = markup.begin(); beg != markup.end(); ++beg )
    {
      fwdFmt.push_back( beg );
      bckFmt.push_back( beg );
    }
    std::sort( bckFmt.begin(), bckFmt.end(), []( textAPI::MarkupTag* a, textAPI::MarkupTag* b )
      {  return a->uUpper < b->uUpper;  } );

    // open and close the items
    auto  fwdtop = fwdFmt.begin();
    auto  bcktop = bckFmt.begin();

    for ( uint32_t windex = 0; windex != uint32_t(tokens.size()); ++windex )
    {
      auto& rfword = tokens[windex];
      auto  uLower = rfword.offset;
      auto  uUpper = windex < tokens.size() - 1 ? tokens[windex + 1].offset : rfword.offset + rfword.length;

      while ( fwdtop != fwdFmt.end() && (*fwdtop)->uLower <= uLower ) (*fwdtop++)->uLower = windex;
      while ( bcktop != bckFmt.end() && (*bcktop)->uUpper <  uUpper ) (*bcktop++)->uUpper = windex;
    }
    while ( fwdtop != fwdFmt.end() )
      (*fwdtop++)->uLower = uint32_t(tokens.size()) - 1;
    while ( bcktop != bckFmt.end() )
      (*bcktop++)->uUpper = uint32_t(tokens.size()) - 1;
  }

}}
