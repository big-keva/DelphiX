# if !defined( __DelphiX_src_indexer_merger_contents_hxx__ )
# define __DelphiX_src_indexer_merger_contents_hxx__
# include "../../contents.hpp"
# include "notify-events.hpp"

namespace DelphiX {
namespace indexer {
namespace fusion {

  class Contents
  {
    std::vector<mtc::api<IContentsIndex>> indexVector;
    Notify::Func                          notifyEvent;
    std::function<bool()>                 canContinue;
    mtc::api<IStorage::IIndexStore>       outputStore;

  public:
    auto  Add( const mtc::api<IContentsIndex> ) -> Contents&;

    auto  Set( Notify::Func ) -> Contents&;
    auto  Set( std::function<bool()> ) -> Contents&;
    auto  Set( mtc::api<IStorage::IIndexStore> ) -> Contents&;
    auto  Set( const mtc::api<IContentsIndex>*, size_t ) -> Contents&;
    auto  Set( const std::vector<mtc::api<IContentsIndex>>& ) -> Contents&;
    auto  Set( const std::initializer_list<mtc::api<IContentsIndex>>& ) -> Contents&;

    auto  Create() -> mtc::api<IContentsIndex>;
  };

}}}

# endif   // !__DelphiX_src_indexer_merger_contents_hxx__
