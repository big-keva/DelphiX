# include "../../enquote/quotations.hpp"
# include "../../context/pack-images.hpp"
# include "../../context/pack-format.hpp"
# include "../../context/text-image.hpp"

namespace DelphiX {
namespace enquote {

  using IText = DeliriX::IText;

  class QuoteMachine::common_settings
  {
    friend class quoter_function;

  public:
    const FieldHandler&   fields;
    std::string           tagBeg = "\x7";
    std::string           tagEnd = "\x8";
    FieldOptions default_options = { unsigned(-1), "", 1.0, 0, { { 2, 8 }, { 4, 8 } } };

  };

  class QuoteMachine::quoter_function
  {
    using TextToken = context::TextToken;
    using MarkupTag = DeliriX::MarkupTag;

    using Abstract = queries::Abstract;
    using EntrySet = Abstract::EntrySet;
    using EntryPos = Abstract::EntryPos;
    using Entries  = Abstract::Entries;

    struct Span
    {
      unsigned  uLower;
      unsigned  uUpper;
      unsigned  points = 0;

      enum: unsigned { loDots = 1, upDots = 2 };
    };

    using Limits = std::vector<Span>;

    std::shared_ptr<common_settings>  common;
    mtc::span<const TextToken>        xwords;
    mtc::span<const MarkupTag>        markup;
    Abstract::Entries                 quotes;

  public:
    quoter_function(
      const std::shared_ptr<common_settings>& coset,
      const mtc::span<const TextToken>&       words,
      const mtc::span<const MarkupTag>&       mkups,
      const Abstract::Entries&                quote );

    void  GetQuotes( IText* ) const;
    void  GetSource( IText* ) const;

  protected:
    void  addQuotes(
      Limits&             output,
      const Span&         bounds,
      const FieldOptions& fdinfo,
      const EntrySet&     entset ) const;
    auto  getBounds() const -> Limits;
    /* getBounds( ... )
     *
     * build the quotation bounds for fhe quoted document with entryset defined
     */
    auto  getBounds(
      Limits&             output,
      const Span&         bounds,
      const FieldOptions& fdinfo,
      const MarkupTag*    format,
      const EntrySet*     quoptr ) const -> std::pair<const MarkupTag*, const EntrySet*>;
    /* getBounds( ... )
     *
     * build the document heading fragments for document with no quotation
     */
    auto  getBounds(
      Limits&             output,
      const Span&         bounds,
      const FieldOptions& fdinfo,
      const MarkupTag*    format ) const -> const MarkupTag*;
    void  getQuotes(
      IText*            output,
      const MarkupTag*& fmtbeg,
      const MarkupTag*  fmtend,
      const Span*&      limbeg,
      const Span*       limend,
      const EntrySet*&  quobeg ) const;
    void  getQuotes(
      IText*            output,
      Span              limits,
      const EntrySet*&  quobeg ) const;
    void  getSource(
      IText*            output,
      const MarkupTag*  fmtbeg,
      Span              bounds ) const;
    auto  getFormat( const MarkupTag* ) const -> MarkupTag;
    auto  loadField( const std::string_view& ) const -> const FieldOptions*;
  };

  // QuoteMachine::quoter_function

  QuoteMachine::quoter_function::quoter_function(
    const std::shared_ptr<common_settings>& coset,
    const mtc::span<const TextToken>&       words,
    const mtc::span<const MarkupTag>&       mkups,
    const Abstract::Entries&                quote ):
      common( coset ),
      xwords( words ),
      markup( mkups ),
      quotes( quote ) {}

  void  QuoteMachine::quoter_function::GetQuotes( IText* output ) const
  {
    auto  limits = getBounds();
    auto  fmtbeg = markup.begin();
    auto  limbeg = (const Span*)limits.data();
    auto  quobeg = quotes.begin();

    return (void)getQuotes( output, fmtbeg, markup.end(), limbeg, limits.data() + limits.size(), quobeg );
  }

  void  QuoteMachine::quoter_function::GetSource( IText* output ) const
  {
    getSource( output, markup.begin(), { 0, unsigned(xwords.size() - 1) } );
  }

  void  QuoteMachine::quoter_function::addQuotes(
    Limits&             output,
    const Span&         bounds,
    const FieldOptions& fdinfo,
    const EntrySet&     entset ) const
  {
    for ( auto& pos: entset.spread )
      if ( bounds.uLower <= pos.offset && bounds.uUpper >= pos.offset )
      {
        auto  aWalls = Span{
          std::max( bounds.uLower, pos.offset - std::min( pos.offset, fdinfo.indents.lower.max ) ),
          std::min( bounds.uUpper, pos.offset + fdinfo.indents.upper.max ) };
        auto  aQuote = Span{
          std::max( aWalls.uLower, pos.offset - std::min( pos.offset, fdinfo.indents.lower.min ) ),
          std::min( aWalls.uUpper, pos.offset + fdinfo.indents.upper.min ) };
        bool  bpoint;

        // move quote limits left and right, markup the dots
        for ( bpoint = false; aQuote.uLower > aWalls.uLower && !(bpoint = xwords[aQuote.uLower - 1].IsPointing()); )
          --aQuote.uLower;
        if ( !bpoint && aQuote.uLower > bounds.uLower )
          aQuote.points |= Span::loDots;

        for ( bpoint = false; aQuote.uUpper < aWalls.uUpper && !bpoint; )
          bpoint = xwords[++aQuote.uUpper].IsPointing();
        if ( !bpoint && aQuote.uUpper < bounds.uUpper )
          aQuote.points |= Span::upDots;

        if ( output.size() != 0 && output.back().uLower >= bounds.uLower && output.back().uUpper + 1 >= aQuote.uLower )
        {
          output.back().points = (output.back().points & ~Span::upDots) | (aQuote.points & Span::upDots);
          output.back().uUpper = aQuote.uUpper;
        } else output.push_back( aQuote );
      }
  }

  auto  QuoteMachine::quoter_function::getBounds() const -> Limits
  {
    Limits  limits;

    if ( !quotes.empty() )
    {
      getBounds( limits, { 0U, unsigned(xwords.size() - 1) }, *loadField( "" ),
        markup.begin(), quotes.begin() );
    }
      else
    {
      getBounds( limits, { 0U, std::min( unsigned(xwords.size() - 1), 25U ) }, *loadField( "" ),
        markup.begin() );
    }

    return limits;
  }

  auto  QuoteMachine::quoter_function::getBounds(
    Limits&             output,
    const Span&         bounds,
    const FieldOptions& fdinfo,
    const MarkupTag*    format,
    const EntrySet*     quoptr ) const -> std::pair<const MarkupTag*, const EntrySet*>
  {
    if ( (fdinfo.options & (FieldOptions::ofEnforceQuote | FieldOptions::ofDisableQuote)) != 0 )
    {
      if ( (fdinfo.options & FieldOptions::ofEnforceQuote) != 0 )
        output.push_back( bounds );

      while ( format != markup.end() && format->uUpper <= bounds.uUpper )
        ++format;
      while ( quoptr != quotes.end() && quoptr->limits.uMax <= bounds.uUpper )
        ++quoptr;

      return { format, quoptr };
    }

  // цикл пока есть форматы или цитаты в заданных пределах
    while ( (format != markup.end() && format->uUpper <= bounds.uUpper)
         || (quoptr != quotes.end() && quoptr->limits.uMin <= bounds.uUpper) )
    {
    // построить цитаты в текущих границах до следующего формата
      while ( quoptr != quotes.end() && quoptr->limits.uMin < getFormat( format ).uLower )
      {
        addQuotes( output, { bounds.uLower, std::min( bounds.uUpper, getFormat( format ).uLower - 1 ) },
          fdinfo, *quoptr );
        if ( quoptr->limits.uMax < getFormat( format ).uLower )
          ++quoptr;
      }

    // прокрутить вложенные форматы до следующей цитаты
      while ( format != markup.end() && format->uUpper <= bounds.uUpper )
      {
        std::tie( format, quoptr ) = getBounds( output, { format->uLower, format->uUpper },
          *loadField( format->tagKey ), format + 1, quoptr );
      }
    }

    return { format, quoptr };
  }

  auto  QuoteMachine::quoter_function::getBounds(
    Limits&             output,
    const Span&         bounds,
    const FieldOptions& fdinfo,
    const MarkupTag*    format ) const -> const MarkupTag*
  {
    if ( (fdinfo.options & (FieldOptions::ofEnforceQuote | FieldOptions::ofDisableQuote)) != 0 )
    {
      if ( (fdinfo.options & FieldOptions::ofEnforceQuote) != 0 )
        output.push_back( bounds );

      while ( format != markup.end() && format->uUpper <= bounds.uUpper )
        ++format;

      return format;
    }

    while ( format != markup.end() )
      format = getBounds( output, { format->uLower, format->uUpper }, *loadField( format->tagKey ), format + 1 );

    return format;
  }

  auto  QuoteMachine::quoter_function::getFormat( const MarkupTag* format ) const -> MarkupTag
  {
    return format != markup.end() ? *format : MarkupTag{ "", unsigned(-1), unsigned(-1) };
  }

  void  QuoteMachine::quoter_function::getQuotes(
    IText*            output,
    const MarkupTag*& fmtbeg,
    const MarkupTag*  fmtend,
    const Span*&      limbeg,
    const Span*       limend,
    const EntrySet*&  quobeg ) const
  {
    while ( limbeg != limend )
    {
      while ( fmtbeg != fmtend && fmtbeg->uUpper < limbeg->uLower )
        ++fmtbeg;

      if ( fmtbeg == fmtend || fmtbeg->uLower > limbeg->uLower )
      {
        getQuotes( output, *limbeg++, quobeg );
      }
        else
      {
        auto  addtag = output->AddMarkupTag( fmtbeg->tagKey );

        getQuotes( addtag, ++fmtbeg, fmtend,
          limbeg, limend, quobeg );
      }
    }
  }

  void  QuoteMachine::quoter_function::getQuotes(
    IText*            output,
    Span              limits,
    const EntrySet*&  quobeg ) const
  {
    auto  thestr = mtc::widestr();
    auto  quoted = [&]( unsigned pos ) -> const EntryPos*
    {
      for ( auto p = quobeg; p != quotes.end() && p->limits.uMin <= pos; ++p )
      {
        if ( p->limits.uMax >= pos )
          for ( auto& wpos: p->spread )
            if ( wpos.offset == pos )
              return &wpos;
      }
      return nullptr;
    };

    if ( limits.points & Span::loDots )
      thestr += widechar( 0x2026 );

    for ( ; limits.uLower <= limits.uUpper; ++limits.uLower )
    {
      auto&  rfword = xwords[limits.uLower];
      auto   marked = quoted( limits.uLower );

      if ( rfword.LeftSpaced() )  thestr += ' ';
        else
      if ( rfword.IsFirstOne() && !thestr.empty() )
      {
        output->AddBlock( thestr );
        thestr.clear();
      }

      if ( marked )
        thestr += codepages::mbcstowide( codepages::codepage_utf8, mtc::strprintf( common->tagBeg.c_str(), marked->termID ) );
      thestr += rfword.GetWideStr();
      if ( marked )
        thestr += codepages::mbcstowide( codepages::codepage_utf8, mtc::strprintf( common->tagEnd.c_str(), marked->termID ) );
    }

    if ( limits.points & Span::upDots )
      thestr += widechar( 0x2026 );

    if ( !thestr.empty() )
      output->AddBlock( thestr );
  }

  auto  QuoteMachine::quoter_function::loadField( const std::string_view& tag ) const -> const FieldOptions*
  {
    const FieldOptions* pfinfo;

    if ( (pfinfo = common->fields.Get( tag )) != nullptr )
      return pfinfo;

    if ( (pfinfo = common->fields.Get( "default_field" )) != nullptr )
      return pfinfo;

    return &common->default_options;
  }

  void  QuoteMachine::quoter_function::getSource(
    IText*            output,
    const MarkupTag*  fmtbeg,
    Span              bounds ) const
  {
    auto  quoted = std::function<const EntryPos*(unsigned)>( []( unsigned ) -> const EntryPos* {  return nullptr;  } );

  // move to first valuable format
    while ( fmtbeg != markup.end() && fmtbeg->uUpper < bounds.uLower )
      ++fmtbeg;

    if ( quotes.pbeg != quotes.pend )
    {
      quoted = [&]( unsigned pos ) -> const EntryPos*
        {
          for ( auto p = quotes.pbeg; p != quotes.pend && p->limits.uMin <= pos; ++p )
            if ( p->limits.uMax >= pos )
              for ( auto u = p->spread.pbeg; u != p->spread.pend && u->offset <= pos; ++u )
                if ( u->offset == pos )
                  return u;
          return nullptr;
        };
    }

  // quotate words in limits passed
    while ( bounds.uLower <= bounds.uUpper )
    {
      auto  thestr = mtc::widestr();

      while ( fmtbeg != markup.end() && fmtbeg->uUpper < bounds.uLower )
        ++fmtbeg;

      for ( ; bounds.uLower <= bounds.uUpper && (fmtbeg == markup.end() || bounds.uLower < fmtbeg->uLower); ++bounds.uLower )
      {
        auto& next = xwords.at( bounds.uLower );
        auto  mark = quoted( bounds.uLower );

        if ( next.LeftSpaced() )
        {
          thestr += widechar( ' ' );
        }
          else
        if ( next.IsFirstOne() && !thestr.empty() )
        {
          output->AddBlock( thestr );
          thestr.clear();
        }

        if ( mark != nullptr )
          thestr += codepages::mbcstowide( codepages::codepage_utf8, mtc::strprintf( common->tagBeg.c_str(), mark->termID ) );
        thestr += next.GetWideStr();
        if ( mark != nullptr )
          thestr += codepages::mbcstowide( codepages::codepage_utf8, mtc::strprintf( common->tagEnd.c_str(), mark->termID ) );
      }

      if ( !thestr.empty() )
        output->AddBlock( thestr );

      if ( bounds.uLower <= bounds.uUpper )
      {
        getSource( output->AddMarkupTag( fmtbeg->tagKey ), fmtbeg + 1,
          { bounds.uLower, std::min( bounds.uUpper, fmtbeg->uUpper ) } );
        bounds.uLower = std::min( bounds.uUpper, fmtbeg->uUpper ) + 1;
      }
    }
  }

  // QuoteMachine implementation

  QuoteMachine::QuoteMachine( const FieldHandler& fds ):
    settings( std::shared_ptr<common_settings>( new common_settings{ fds } ) ) {}

  auto  QuoteMachine::SetLabels( const char* open, const char* close ) -> QuoteMachine&
  {
    settings->tagBeg = open;
    settings->tagEnd = close;
    return *this;
  }

  auto  QuoteMachine::SetIndent( const FieldOptions::indentation& indent ) -> QuoteMachine&
  {
    settings->default_options.indents = indent;
    return *this;
  }

  auto  QuoteMachine::Structured() -> QuotesFunc
  {
    return [opts = settings](
      IText*                        output,
      const mtc::span<const char>&  imgsrc,
      const mtc::span<const char>&  fmtsrc,
      const queries::Abstract&      quotes )
    {
      auto  ximage = context::Image();
      auto  addTag = [&]( const context::formats::RankerTag& tag )
      {
        auto  pf = opts->fields.Get( tag.format );

        if ( pf != nullptr )
          ximage.GetMarkup().push_back( { pf->name.data(), tag.uLower, tag.uUpper } );
      };

      context::imaging::Unpack( ximage, imgsrc );
      context::formats::Unpack( addTag, fmtsrc );

      return quoter_function( opts, ximage.GetTokens(), ximage.GetMarkup(), GetQuotation( quotes ) )
        .GetQuotes( output );
    };
  }

  auto  QuoteMachine::TextSource() -> QuotesFunc
  {
    return [opts = settings](
      IText*                        output,
      const mtc::span<const char>&  imgsrc,
      const mtc::span<const char>&  fmtsrc,
      const queries::Abstract&      quotes )
    {
      auto  ximage = context::Image();
      auto  addTag = [&]( const context::formats::RankerTag& tag )
      {
        auto  pf = opts->fields.Get( tag.format );

        if ( pf != nullptr )
          ximage.GetMarkup().push_back( { pf->name.data(), tag.uLower, tag.uUpper } );
      };

      context::imaging::Unpack( ximage, imgsrc );
      context::formats::Unpack( addTag, fmtsrc );

      return quoter_function( opts, ximage.GetTokens(), ximage.GetMarkup(), GetQuotation( quotes ) )
        .GetSource( output );
    };
  }

}}


