# if !defined( __DelphiX_src_indexer_dynamic_chains_ringbuffer_hxx__ )
# define __DelphiX_src_indexer_dynamic_chains_ringbuffer_hxx__
# include <cstddef>
# include <atomic>

namespace DelphiX {
namespace indexer {
namespace dynamic {

  template <class T, size_t N>
  class RingBuffer
  {
    using AtomicValue = std::atomic<T>;
    using AtomicPlace = std::atomic<AtomicValue*>;

    AtomicValue   buffer[N];
    AtomicPlace   buftop = &buffer[0];
    AtomicPlace   bufend = &buffer[0];

  protected:
    auto  next( std::atomic<T>* p ) -> std::atomic<T>*
    {
      return ++p < &buffer[N] ? p : &buffer[0];
    }

  public:
    void  Put( T p )
    {
      for ( ; ; )
      {
      // allocate space for possible next value in buffer;
        auto  pfetch = buftop.load();
        auto  pstore = bufend.load();
        auto  pafter = next( pstore );

      // check if rotated buffer stops to reader buffer
        if ( pafter != pfetch && bufend.compare_exchange_weak( pstore, pafter ) )
          return (void)pstore->store( p );
      }
    }
    bool  Get( T& tvalue )
    {
      for ( ; ; )
      {
        auto  pfetch = buftop.load();
        auto  pstore = bufend.load();
        auto  pafter = next( pfetch );

        if ( pfetch == pstore )
          return false;
        if ( buftop.compare_exchange_weak( pfetch, pafter ) )
          return (tvalue = pfetch->load()), true;
      }
    }
  };

}}}

# endif   // !__DelphiX_src_indexer_dynamic_chains_ringbuffer_hxx__
