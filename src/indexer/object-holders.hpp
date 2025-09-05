# include <mtc/interfaces.h>

namespace DelphiX {
namespace indexer {

  template <class ... Holders>
  class ObjectHolder final: public mtc::Iface, protected std::tuple<Holders...>
  {
    implement_lifetime_control

    ObjectHolder( Holders&&... args ): std::tuple<Holders...>( std::forward<Holders>( args )... ) {}
  };

  template <class ... Holders>
  auto  MakeObjectHolder( Holders&&... args )
  {
    return new ObjectHolder<Holders...>( std::forward<Holders>( args )... );
  }

}}
