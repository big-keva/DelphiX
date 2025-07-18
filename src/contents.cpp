# include "../contents.hpp"

namespace DelphiX {

  class Lister: public IContentsIndex::IIndexAPI
  {
    std::function<void( const StrView&, const StrView&, unsigned )> forward;

  public:
    Lister( std::function<void( const StrView&, const StrView&, unsigned )> fn ):
      forward( fn ) {}

  public:
    void  Insert( const StrView& key, const StrView& block, unsigned bkType ) override
    {
      forward( key, block, bkType );
    }

    auto  ptr() -> IIndexAPI* {  return static_cast<IIndexAPI*>( this );  }

  };

  // StrView implementation

  StrView::StrView( const char* pch )
  {
    for ( items = pch, count = 0; pch[count] != 0; ++count  )
      (void)NULL;
  }

  StrView::StrView( mtc::api<const mtc::IByteBuffer> buf ):
    StrView( buf != nullptr ? buf->GetPtr() : nullptr, buf != nullptr ? buf->GetLen() : 0 )
  {
  }

  // IContents implementation

  void  IContents::List( std::function<void( const StrView&, const StrView&, unsigned )> fn )
  {
    Enum( Lister( fn ).ptr() );
  }

}
