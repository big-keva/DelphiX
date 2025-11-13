# include "../contents.hpp"

namespace DelphiX {

  class Lister: public IContentsIndex::IIndexAPI
  {
    std::function<void( const std::string_view&, const std::string_view&, unsigned )> forward;

  public:
    Lister( std::function<void( const std::string_view&, const std::string_view&, unsigned )> fn ):
      forward( fn ) {}

  public:
    void  Insert( const std::string_view& key, const std::string_view& block, unsigned bkType ) override
    {
      forward( key, block, bkType );
    }

    auto  ptr() -> IIndexAPI* {  return static_cast<IIndexAPI*>( this );  }

  };

  // IContents implementation

  void  IContents::List( std::function<void( const std::string_view&, const std::string_view&, unsigned )> fn )
  {
    Enum( Lister( fn ).ptr() );
  }

}
