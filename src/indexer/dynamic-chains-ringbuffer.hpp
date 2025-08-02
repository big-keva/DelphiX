# if !defined( __DelphiX_src_indexer_dynamic_chains_ringbuffer_hxx__ )
# define __DelphiX_src_indexer_dynamic_chains_ringbuffer_hxx__
# include <mtc/ptr.h>
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
    void  Put( T t )
    {
      for ( auto pstore = mtc::ptr::clean( bufend.load() ); ; pstore = mtc::ptr::clean( pstore ) )
      {
        auto  pafter = next( pstore );
        auto  pfetch = mtc::ptr::clean( buftop.load() );

        if ( pafter != pfetch )
        {
          if ( bufend.compare_exchange_weak( pstore, mtc::ptr::dirty( pstore ) ) )
            return (void)(pstore->store( t ), bufend = pafter);
        }
      }
    }
    bool  Get( T& tvalue )
    {
      for ( auto  pfetch = mtc::ptr::clean( buftop.load() ); ; pfetch = mtc::ptr::clean( pfetch ) )
      {
        auto  pstore = mtc::ptr::clean( bufend.load() );
        auto  pafter = next( pfetch );

        if ( pfetch == pstore )
          return false;

        // make dirty fetch pointer
        if ( buftop.compare_exchange_strong( pfetch, mtc::ptr::dirty( pfetch ) ) )
          return tvalue = pfetch->load(), buftop = pafter, true;
      }
    }
  };

}}}

# endif   // !__DelphiX_src_indexer_dynamic_chains_ringbuffer_hxx__
