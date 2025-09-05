# if !defined( __DelphiX_src_indexer_dynamic_chains_hxx__ )
# define __DelphiX_src_indexer_dynamic_chains_hxx__
# include "../../contents.hpp"
# include "dynamic-chains-ringbuffer.hpp"
# include "dynamic-bitmap.hpp"
# include "strmatch.hpp"
# include <mtc/recursive_shared_mutex.hpp>
# include <mtc/radix-tree.hpp>
# include <condition_variable>
# include <thread>
# include <atomic>

# if defined( VERIFY_KEY_COUNT )
#   include <cassert>
# endif

namespace DelphiX {
namespace indexer {
namespace dynamic {

  enum: size_t
  {
    ring_buffer_size = 0x1000
  };

  template <class Allocator = std::allocator<char>>
  class BlockChains
  {
    struct ChainLink;
    struct ChainHook;

    using AtomicLink = std::atomic<ChainLink*>;
    using AtomicHook = std::atomic<ChainHook*>;

    using LinkAllocator = typename std::allocator_traits<Allocator>::rebind_alloc<ChainLink>;
    using HookAllocator = typename std::allocator_traits<Allocator>::rebind_alloc<ChainHook>;

    enum: size_t
    {
      hash_table_size = 40013
    };

  /*
   * ChainLink represents one entity identified by virtual index iEnt data block
   * linked in a chain of blocks in increment order of entity indices
   */
    struct ChainLink
    {
      AtomicLink  p_next = nullptr;
      uint32_t    entity;
      uint32_t    lblock;

    public:
      template <class T>
      ChainLink( uint32_t ent, const T& blk ): entity( ent )
        {  memcpy( data(), blk.data(), lblock = blk.size() );  }

    public:
      auto  data() const -> const char*  {  return (const char*)(this + 1);  }
      auto  data() ->             char*  {  return (      char*)(this + 1);  }

    };

  /*
   * ChainHook holds key body and reference to the first element in the chain
   * of blocks indexed by incremental virtual entity indices
   */
    struct ChainHook
    {
      using LastAnchor = std::atomic<AtomicLink*>;

      enum: size_t
      {
        cache_size = 32,
        cache_step = 64
      };

      const unsigned        bkType;
      LinkAllocator         malloc;
      AtomicHook            pchain;               // collisions

      AtomicLink            pfirst = nullptr;     // first in chain
      AtomicLink            points[cache_size];   // points cache
      LastAnchor            ppoint = points - 1;  // invalid value
      std::atomic_uint32_t  ncount = 0;

      size_t                cchkey;               // key length

    public:
      auto  data() const -> const char* {  return (const char*)(this + 1);  }
      auto  data() -> char* {  return (char*)(this + 1);  }

    public:
      ChainHook( const StrView& key, unsigned blockType, ChainHook*, Allocator );
     ~ChainHook();

    public:
      bool  operator == ( const StrView& s ) const
        {  return cchkey == s.size() && memcmp( data(), s.data(), s.size() ) == 0;  }

    public:
      void  Insert( uint32_t entity, const StrView& block );
      void  Markup();
     /*
      * bool  Verify() const;
      *
      * for tesing:
      *   verifies the chain of entities to increment values
      *   element to element
      */
      bool  Verify() const;
      template <class OtherAllocator>
      auto  Remove( const Bitmap<OtherAllocator>& ) -> ChainHook&;

    };

  public:
    class KeyLister;

    BlockChains( Allocator alloc = Allocator() );
   ~BlockChains();

    void  Insert( const StrView& key, uint32_t entity, const StrView& block, unsigned bkType );
    auto  Lookup( const StrView& key ) const -> const ChainHook*;
    template <class OtherAllocator>
    auto  Remove( const Bitmap<OtherAllocator>& ) -> BlockChains&;
    auto  StopIt() -> BlockChains&;

    auto  KeySet( const StrView& ) const -> KeyLister;

   /*
    * bool  Verify() const;
    *
    * for tesing: lists all the keys and checks integrity
    */
    bool  Verify() const;

  public:     //serialization
    template <class O1, class O2>
    bool  Serialize( O1*, O2* );
    bool  VerifyIds( unsigned ) const;

  protected:
    void  KeysIndexer();

  protected:
    struct RadixLink
    {
      ChainHook*  blocksChain;
      uint64_t    blockOffset;
      uint32_t    blockLength;

      size_t  GetBufLen() const
      {
        return ::GetBufLen( blocksChain->bkType )
             + ::GetBufLen( blocksChain->ncount.load() )
             + ::GetBufLen( blockOffset )
             + ::GetBufLen( blockLength );
      }
      template <class O>
      O*    Serialize( O* o ) const
      {
        return ::Serialize( ::Serialize( ::Serialize( ::Serialize( o,
          blocksChain->bkType ), blocksChain->ncount.load() ), blockOffset ), blockLength );
      }
    };

    std::vector<AtomicHook, Allocator>        hashTable;
    HookAllocator                             hookAlloc;

    mtc::radix::tree<RadixLink, Allocator>    radixTree;    // parallel radix tree
    mutable std::shared_mutex                 radixLock;    // locker to access

    RingBuffer<ChainHook*, ring_buffer_size>  keysQueue;    // queue for keys indexing
    std::condition_variable                   keySyncro;    // syncro for shadow indexing keys
    std::thread                               keyThread;    // shadow keys indexer
    volatile bool                             runThread = false;

  };

  template <class Allocator>
  class BlockChains<Allocator>::KeyLister
  {
    using const_it_type = typename mtc::radix::tree<RadixLink, Allocator>::
      const_iterator<std::allocator<char>>;

  public:
    KeyLister( std::shared_mutex&, const_it_type&&, const_it_type&&, std::string&& );
    KeyLister( std::shared_mutex&, const_it_type&&, const_it_type&& );

    auto  CurrentKey() -> std::string;
    auto  GetNextKey() -> std::string;

  protected:
    std::shared_lock<std::shared_mutex> lck;
    const_it_type                       beg;
    const_it_type                       end;
    std::string                         tpl;

  };

// KeyBlockChains template implementation

  template <class Allocator>
  BlockChains<Allocator>::BlockChains( Allocator alloc ):
    hashTable( hash_table_size, alloc ),
    hookAlloc( alloc ),
    radixTree( alloc )
  {
    for ( keyThread = std::thread( &BlockChains<Allocator>::KeysIndexer, this ); !runThread; )
      std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
  }

  template <class Allocator>
  BlockChains<Allocator>::~BlockChains()
  {
    StopIt();

    for ( auto& next: hashTable )
      for ( auto tostep = next.load(), tofree = tostep; tofree != nullptr; tofree = tostep )
      {
        tostep = tofree->pchain.load();
          tofree->~ChainHook();
        hookAlloc.deallocate( tofree, 0 );
      }
  }

  template <class Allocator>
  void  BlockChains<Allocator>::Insert( const StrView& key, uint32_t entity, const StrView& block, unsigned bkType )
  {
    auto  hindex = std::hash<std::string_view>{}( { key.data(), key.size() } ) % hashTable.size();
    auto  hentry = &hashTable[hindex];
    auto  hvalue = mtc::ptr::clean( hentry->load() );

  // check block type; set the type value if is not set yet
    if ( bkType == unsigned(-1) )
      bkType = block.size() != 0 ? 0x10 : 0;

  // first try find existing block in the hash chain
    for ( ; hvalue != nullptr; hvalue = hvalue->pchain.load() )
      if ( *hvalue == key )
      {
        if ( hvalue->bkType != bkType )
          throw std::invalid_argument( "Block type do not match the previously defined type" );
        return hvalue->Insert( entity, block );
      }

  // now try lock the hash table entry to create record
    for ( hvalue = mtc::ptr::clean( hentry->load() ); !hentry->compare_exchange_weak( hvalue, mtc::ptr::dirty( hvalue ) ); )
      hvalue = mtc::ptr::clean( hvalue );

  // check if locked hvalue list still contains no needed key; unlock and finish
  // if key is now found
    for ( ; hvalue != nullptr; hvalue = hvalue->pchain.load() )
      if ( *hvalue == key )
      {
        hentry->store( mtc::ptr::clean( hentry->load() ) );

        if ( hvalue->bkType != bkType )
          throw std::invalid_argument( "Block type do not match the previously defined type" );
        return hvalue->Insert( entity, block );
      }

  // list contains no needed entry; allocate new ChainHook for new key;
  // for possible exceptions, unlock the entry and continue tracing
    try
    {
      new( hvalue = hookAlloc.allocate( (sizeof(ChainHook) * 2 + key.size() - 1) / sizeof(ChainHook) ) )
        ChainHook( key, bkType, mtc::ptr::clean( hentry->load() ), hookAlloc );

      hentry->store( hvalue );

      keysQueue.Put( hvalue );
      keySyncro.notify_one();
    }
    catch ( ... )
    {
      hentry->store( mtc::ptr::clean( hentry->load() ) );
      throw;
    }

    return hvalue->Insert( entity, block );
  }

  template <class Allocator>
  auto  BlockChains<Allocator>::Lookup( const StrView& key ) const -> const ChainHook*
  {
    auto  hindex = std::hash<std::string_view>{}( { key.data(), key.size() } ) % hashTable.size();
    auto  hvalue = mtc::ptr::clean( hashTable[hindex].load() );

  // first try find existing block in the hash chain
    for ( ; hvalue != nullptr; hvalue = hvalue->pchain.load() )
      if ( *hvalue == key )
        return hvalue;

    return nullptr;
  }

  template <class Allocator>
  auto  BlockChains<Allocator>::StopIt() -> BlockChains&
  {
    if ( keyThread.joinable() )
    {
      while ( !runThread )
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );

      runThread = false;
      keySyncro.notify_one();
      keyThread.join();
    }
    return *this;
  }

  template <class Allocator>
  auto  BlockChains<Allocator>::KeySet( const StrView& key ) const -> KeyLister
  {
    auto  templStr = std::string( key.begin(), key.end() );
    auto  templLen = size_t(0);
    auto  radixBeg = radixTree.cend( std::allocator<char>() );
    auto  radixEnd = radixTree.cend( std::allocator<char>() );

    for ( ; templLen < templStr.size() && templStr[templLen] != '*' && templStr[templLen] != '?'; ++templLen )
      (void)NULL;

    if ( templLen != 0 )  radixBeg = radixTree.lower_bound( { templStr.data(), templLen }, std::allocator<char>() );
      else radixBeg = radixTree.cbegin( std::allocator<char>() );

    return KeyLister( radixLock, std::move( radixBeg ), std::move( radixEnd ), std::move( templStr ) );
  }

  template <class Allocator>
  template <class OtherAllocator>
  auto  BlockChains<Allocator>::Remove( const Bitmap<OtherAllocator>& deleted ) -> BlockChains&
  {
    for ( auto it = radixTree.begin(); it != radixTree.end(); )
    {
      if ( it->second.blocksChain->Remove( deleted ).ncount == 0 )
        it = radixTree.erase( it );
      else ++it;
    }
    return *this;
  }

  template <class Allocator>
  bool  BlockChains<Allocator>::Verify() const
  {
    for ( auto& next: hashTable )
      for ( auto verify = next.load(); verify != nullptr; verify = verify->pchain.load() )
        if ( !verify->Verify() )
          return false;
    return true;
  }

 /*
  * Serialize( index, chain )
  *
  * Serializes the created inverted index to storage.
  */
  template <class Allocator>
  template <class O1, class O2>
  bool  BlockChains<Allocator>::Serialize( O1* index, O2* chain )
  {
    uint64_t  offset = 0;

# if defined( VERIFY_KEY_COUNT )
    // для уверенности в том, что KeysIndexer ничего не промотал, проверить совпадение количества
    // ключей в hash-table и в radixTree
    for ( auto& next: hashTable )
      for ( auto tostep = next.load(); tostep != nullptr; tostep = tostep->pchain.load() )
        assert( radixTree.Search( { tostep->data(), tostep->cchkey } ) != nullptr );
# endif   // VERIFY_KEY_COUNT

  // store all the index chains saving offset, count and length to the tree
    for ( auto next = radixTree.begin(), stop = radixTree.end(); next != stop && chain != nullptr; ++next )
    {
      auto  lastId = uint32_t(0);
      auto  length = uint32_t(0);

      next->value.blockOffset = offset;

    // store block acctoding to block type:
    //  * blocks without coordinates;
    //  * blocks with coordinates
      if ( next->value.blocksChain->bkType == 0 )
      {
        for ( auto p = next->value.blocksChain->pfirst.load(); p != nullptr; p = p->p_next.load() )
          if ( p->entity != uint32_t(-1) )
          {
            auto  diffId = p->entity - lastId - 1;

            length += ::GetBufLen( diffId );
              chain = ::Serialize( chain, diffId );
            lastId = p->entity;
          }
      }
        else
      {
        for ( auto p = next->value.blocksChain->pfirst.load(); p != nullptr; p = p->p_next.load() )
          if ( p->entity != uint32_t(-1) )
          {
            auto  diffId = p->entity - lastId - 1;
            auto  blkLen = p->lblock;

            length += ::GetBufLen( diffId ) + p->lblock + ::GetBufLen( blkLen );
              chain = ::Serialize( ::Serialize( ::Serialize( chain,
                diffId ),
                blkLen ), p->data(), blkLen );
            lastId = p->entity;
          }
      }

      offset += (next->value.blockLength = length);
    }

  // store radix tree
    return chain != nullptr && (index = radixTree.Serialize( index )) != nullptr;
  }

  template <class Allocator>
  bool  BlockChains<Allocator>::VerifyIds( unsigned maxIndex ) const
  {
    // store all the index chains saving offset, count and length to the tree
    for ( auto next = radixTree.begin(), stop = radixTree.end(); next != stop; ++next )
    {
      for ( auto p = next->value.blocksChain->pfirst.load(); p != nullptr; p = p->p_next.load() )
        if ( p->entity != uint32_t(-1) && p->entity > maxIndex )
          return false;
    }
    return true;
  }

 /*
  * Shadow keys indexer
  *
  * Wakes up on signals, indexes all the new keys from the queue, stops after
  * all the keys are indexed and runThread is false.
  */
  template <class Allocator>
  void BlockChains<Allocator>::KeysIndexer()
  {
    std::mutex  waitex;
    auto        locker = mtc::make_unique_lock( waitex );
    ChainHook*  addkey;

    pthread_setname_np( pthread_self(), "KeysIndexer" );

    for ( runThread = true; runThread; )
    {
      auto  modify = mtc::make_unique_lock( radixLock, std::defer_lock );

      for ( keySyncro.wait( locker ), modify.lock(); keysQueue.Get( addkey ); )
        radixTree.Insert( { addkey->data(), addkey->cchkey }, { addkey, 0, 0 } );
    }
    while ( keysQueue.Get( addkey ) )
      radixTree.Insert( { addkey->data(), addkey->cchkey }, { addkey, 0, 0 } );
  }

  // KeyBlockChains::ChainHook template implementation

  template <class Allocator>
  BlockChains<Allocator>::ChainHook::ChainHook( const StrView& key, unsigned b, ChainHook* p, Allocator m ):
    bkType( b ),
    malloc( m ),
    pchain( p )
  {
    memcpy( data(), key.data(), cchkey = key.size() );
  }

  template <class Allocator>
  BlockChains<Allocator>::ChainHook::~ChainHook()
  {
    for ( auto pnext = pfirst.load(), pfree = pnext; pfree != nullptr; pfree = pnext )
    {
      pnext = pfree->p_next.load();
        pfree->~ChainLink();
      malloc.deallocate( pfree, 0 );
    }
  }

  template <class Allocator>
  void  BlockChains<Allocator>::ChainHook::Insert( uint32_t entity, const StrView& block )
  {
    auto        newptr = new( malloc.allocate( (sizeof(ChainLink) * 2 + block.size() - 1) / sizeof(ChainLink) ) )
      ChainLink( entity, block );
    ChainLink*  pentry;
    AtomicLink* anchor;

  // check if no elements available and try write first element;
  // if succeeded, increment element count and return
    if ( pfirst.compare_exchange_weak( pentry = nullptr, newptr ) )
      return (void)++ncount;

  // now pentry has the value != nullptr that was stored in pfirst
  // on the moment of a call
  //  if ( pentry == nullptr )
  //    throw std::logic_error( "algorithm error @" __FILE__ ":" LineId( __LINE__ ) );

  // check if index is not being built this moment and get the latest
  // insert position
    if ( mtc::ptr::is_clean( anchor = ppoint.load() ) )
    {
      while ( anchor >= points && (pentry = (anchor--)->load())->entity >= entity )
        (void)NULL;
      if ( pentry->entity >= entity )
        pentry = pfirst.load();
    }

  // check if the element inserted < than the first element stored;
  // try prepend current element to the existing list
    while ( entity < pentry->entity )
    {
      if ( newptr->p_next = pentry, pfirst.compare_exchange_weak( pentry, newptr ) )
        return (++ncount % cache_step) == 0 ? Markup() : (void)NULL;
    }

  // now the element inserted is > pentry;
  //  if ( pentry == nullptr || entity <= pentry->entity )
  //    throw std::logic_error( "algorithm error @" __FILE__ ":" LineId( __LINE__ ) );

  // follow the list searching for element to be appended to current element
    for ( ; ; )
    {
      auto  follow = pentry->p_next.load();

      if ( follow == nullptr || follow->entity >= entity )
      {
        newptr->p_next = follow;

        if ( pentry->p_next.compare_exchange_weak( follow, newptr ) )
          return (++ncount % cache_step) == 0 ? Markup() : (void)NULL;
      }
      if ( follow->entity <= entity )
        pentry = follow;
    }
  }

  template <class Allocator>
  void  BlockChains<Allocator>::ChainHook::Markup()
  {
    auto  n_gran = ncount.load() / cache_size;
    auto  pentry = pfirst.load();
    auto  pcache = mtc::ptr::clean( ppoint.load() );

    // ensure only one cache builder
    if ( ppoint.compare_exchange_weak( pcache, mtc::ptr::dirty( pcache ) ) ) pcache = points;
      else return;

    for ( size_t nindex = 0; pentry != nullptr; pentry = pentry->p_next.load() )
      if ( nindex++ == n_gran )
      {
        *pcache++ = pentry;
        nindex = 0;
      }
    ppoint = pcache - 1;
  }

  template <class Allocator>
  bool  BlockChains<Allocator>::ChainHook::Verify() const
  {
    auto  entity = uint32_t(0);

    for ( auto pentry = pfirst.load(); pentry != nullptr; pentry = pentry->p_next.load() )
      if ( pentry->entity <= entity )
        return false;
      else entity = pentry->entity;

    return true;
  }

  template <class Allocator>
  template <class OtherAllocator>
  auto  BlockChains<Allocator>::ChainHook::Remove( const Bitmap<OtherAllocator>& deleted ) -> ChainHook&
  {
    for ( auto p = pfirst.load(); p != nullptr; p = p->p_next.load() )
      if ( deleted.Get( p->entity ) )
      {
        p->entity = uint32_t(-1);
        --ncount;
      }
    return *this;
  }

  // BlockChains::KeyLister implementation

  template <class Allocator>
  BlockChains<Allocator>::KeyLister::KeyLister( std::shared_mutex& mx,
    const_it_type&& ib,
    const_it_type&& ie, std::string&& tp ):
      lck( mx ),
      beg( std::move( ib ) ),
      end( std::move( ie ) ),
      tpl( std::move( tp ) )
  {
    if ( tpl.length() != 0 )
    {
      int   rescmp;

      while ( beg != end && (rescmp = strmatch( tpl, beg->key )) < 0 )
        ++beg;
      if ( rescmp != 0 )
        beg = end;
    }
  }

  template <class Allocator>
  BlockChains<Allocator>::KeyLister::KeyLister( std::shared_mutex& mx,
    const_it_type&& ib,
    const_it_type&& ie ):
      lck( mx ),
      beg( std::move( ib ) ),
      end( std::move( ie ) ) {}

  template <class Allocator>
  auto  BlockChains<Allocator>::KeyLister::CurrentKey() -> std::string
  {
    return beg != end ? beg->key.to_string() : "";
  }

  template <class Allocator>
  auto  BlockChains<Allocator>::KeyLister::GetNextKey() -> std::string
  {
    if ( beg != end )
    {
      int   rescmp;

      do ++beg;
        while ( beg != end && (rescmp = strmatch( tpl, beg->key )) < 0 );
      if ( rescmp != 0 )
        beg = end;
    }
    return beg != end ? beg->key.to_string() : "";
  }

}}}

# endif   // __DelphiX_src_indexer_dynamic_chains_hxx__
