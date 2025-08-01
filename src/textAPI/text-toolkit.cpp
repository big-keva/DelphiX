# include "../../text-api.hpp"
# include <functional>

namespace DelphiX {
namespace textAPI {

  class UtfTxt final: public IText
  {
    implement_lifetime_control

  public:
    UtfTxt( mtc::api<IText> tx, unsigned cp ):
      output( tx ),
      encode( cp )  {}

    auto  AddTextTag( const char* tag, size_t len ) -> mtc::api<IText> override
    {
      return new UtfTxt( output->AddTextTag( tag, len ), encode );
    }
    void  AddCharStr( const char* str, size_t len, unsigned enc ) override
    {
      widechar  wcsstr[0x400];

      if ( enc != codepages::codepage_utf8 && len <= std::size( wcsstr ) )
        return output->AddWideStr( wcsstr, codepages::mbcstowide( enc, wcsstr, str, len ) );

      return output->AddString( codepages::mbcstowide( enc != 0 ? enc : encode, str, len ) );
    }
    void  AddWideStr( const widechar* str, size_t len ) override
    {
      return output->AddWideStr( str, len );
    }

  protected:
    mtc::api<IText> output;
    unsigned        encode;

  };

  bool  IsEncoded( const ITextView& textview, unsigned codepage )
  {
    for ( auto& next: textview.GetBlocks() )
      if ( next.encode != codepage )
        return false;
    return true;
  }

  auto  CopyUtf16( IText* output, const ITextView& source, unsigned encode ) -> IText*
  {
    UtfTxt  utfOut( output, encode );
      utfOut.Attach();
    return Serialize( &utfOut, source );
  }

  auto  Serialize( IText* output, const ITextView& source ) -> IText*
  {
    auto  lineIt = source.GetBlocks().begin();
    auto  spanIt = source.GetMarkup().begin();
    auto  offset = uint32_t(0);
    auto  fPrint = std::function<void( IText*, uint32_t )>();

    fPrint = [&]( IText* to, uint32_t up )
      {
        while ( lineIt != source.GetBlocks().end() && offset < up )
        {
        // check if print next line to current IText*
          if ( spanIt == source.GetMarkup().end() || offset < spanIt->uLower )
          {
            if ( lineIt->encode == unsigned(-1) )
              to->AddString( lineIt->GetWideStr() );
            else
              to->AddString( lineIt->GetCharStr(), lineIt->encode );

            offset += lineIt->length;

            if ( ++lineIt == source.GetBlocks().end() )  return;
              continue;
          }

        // check if open new span
          if ( offset >= spanIt->uLower )
          {
            auto  new_to = to->AddTextTag( spanIt->format );
            auto  uUpper = spanIt->uUpper;
              ++spanIt;
            fPrint( new_to.ptr(), uUpper );
          }
        }
      };

    return fPrint( output, uint32_t(-1) ), output;
  }

}}
