# if !defined( __DelphiX_src_indexer_notify_events_hxx__ )
# define __DelphiX_src_indexer_notify_events_hxx__
# include <functional>

namespace DelphiX {

  struct Notify final
  {
    enum class Event: unsigned
    {
      None = 0,
      OK = 1,
      Empty = 2,
      Canceled = 3,
      Failed = 4
    };

    using Func = std::function<void(void*, Event)>;

  };

}

# endif   // !__DelphiX_src_indexer_notify_events_hxx__
