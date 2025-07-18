# include "index-layers.hpp"
# include "override-entities.hpp"

namespace DelphiX {
namespace indexer {

  class IndexLayers::Entities final: public IContentsIndex::IEntities
  {
    struct BlockEntry
    {
      uint32_t            uLower;
      uint32_t            uUpper;
      mtc::api<IEntities> entSet;
    };

    using BlockSet = std::vector<BlockEntry>;

    mtc::api<const mtc::Iface>        holder;
    BlockSet                          blocks;
    mutable BlockSet::const_iterator  pblock;
    uint32_t                          ncount = 0;
    uint32_t                          bktype = uint32_t(-1);

    implement_lifetime_control

  public:
    Entities( const mtc::Iface* parent = nullptr );

    void  AddBlock( const BlockEntry& );

  public:
    auto  Find( uint32_t ) -> Reference override;
    auto  Size() const -> uint32_t override {  return ncount;  }
    auto  Type() const -> uint32_t override {  return bktype;  }

  };

  // IndexLayers implementation

  IndexLayers::IndexLayers( const mtc::api<IContentsIndex>* indices, size_t count )
  {
    uint32_t  uLower = 1;

    for ( auto end = indices + count; indices != end; uLower += (*indices++)->GetMaxIndex() )
      layers.emplace_back( uLower, *indices );
  }

 /*
  * getEntity( id )
  *
  * Returns entity from one of indices held which is not excluded from by other indices
  * with override process.
  *
  * The resulted entity has overriden index.
  */
  auto  IndexLayers::getEntity( EntityId id ) const -> mtc::api<const IEntity>
  {
    for ( auto beg = layers.rbegin(); beg != layers.rend(); ++beg )
    {
      auto  entity = beg->pIndex->GetEntity( id );

      if ( entity != nullptr )
        return beg->Override( entity );
    }
    return {};
  }

  auto  IndexLayers::getEntity( uint32_t ix ) const -> mtc::api<const IEntity>
  {
    for ( auto& next: layers )
      if ( next.uLower <= ix && next.uUpper >= ix )
        return next.pIndex->GetEntity( ix - next.uLower + 1 );

    return {};
  }

  bool  IndexLayers::delEntity( EntityId id )
  {
    auto  deleted = false;

    for ( auto& next: layers )
      deleted |= next.pIndex->DelEntity( id );
    return deleted;
  }

  auto  IndexLayers::setExtras( EntityId id, const StrView& xtras ) -> mtc::api<const IEntity>
  {
    auto  entity = mtc::api<const IEntity>();

    for ( auto& next: layers )
      if ( (entity = next.pIndex->SetExtras( id, xtras )) != nullptr )
        return entity;

    return {};
  }

  auto  IndexLayers::getMaxIndex() const -> uint32_t
  {
    return layers.size() != 0 ? layers.back().uUpper : 0;
  }

  auto  IndexLayers::getKeyBlock( const StrView& key, const mtc::Iface* pix ) const -> mtc::api<IContentsIndex::IEntities>
  {
    mtc::api<Entities>  entities;

    for ( auto& next: layers )
    {
      auto  pblock = next.pIndex->GetKeyBlock( key );

      if ( pblock != nullptr )
      {
        if ( entities == nullptr )
          entities = new Entities( pix );

        entities->AddBlock( { next.uLower, next.uUpper, pblock } );
      }
    }
    return entities.ptr();
  }

  auto  IndexLayers::getKeyStats( const StrView& key ) const -> IContentsIndex::BlockInfo
  {
    IContentsIndex::BlockInfo blockStats = { uint32_t(-1), 0 };

    for ( auto& next: layers )
    {
      auto  cStats = next.pIndex->GetKeyStats( key );

      if ( blockStats.bkType == uint32_t(-1) )  blockStats = cStats;
        else
      if ( blockStats.bkType == cStats.bkType ) blockStats.nCount += cStats.nCount;
        else
      throw std::invalid_argument( "Block types differ in sequental indives" );
    }

    return blockStats;
  }

  void  IndexLayers::addContents( mtc::api<IContentsIndex> ix )
  {
    auto  uLower = layers.empty() ? 1 : layers.back().uUpper + 1;

    layers.emplace_back( uLower, ix );
  }

  void  IndexLayers::commitItems()
  {
    for ( auto& next: layers )
      next.pIndex->Commit();
  }

  void  IndexLayers::hideClashes()
  {
    for ( auto beg = layers.begin(); beg != layers.end(); ++beg )
    {
    }
  }

  // IndexLayers::IndexEntry implementation

  IndexLayers::IndexEntry::IndexEntry( uint32_t lower, mtc::api<IContentsIndex> index ):
    uLower( lower ),
    uUpper( uLower + index->GetMaxIndex() - 1 ),
    pIndex( index )
  {
  }

  IndexLayers::IndexEntry::IndexEntry( const IndexEntry& ie ):
    uLower( ie.uLower ),
    uUpper( ie.uUpper ),
    pIndex( ie.pIndex )
  {
  }

  IndexLayers::IndexEntry::IndexEntry( IndexEntry&& ie ):
    uLower( ie.uLower ),
    uUpper( ie.uUpper ),
    pIndex( ie.pIndex )
  {
  }

  auto  IndexLayers::IndexEntry::operator=( const IndexEntry& ie ) -> IndexEntry&
  {
    uLower = ie.uLower;
    uUpper = ie.uUpper;
    pIndex = ie.pIndex;
    return *this;
  }

  auto  IndexLayers::IndexEntry::operator=( IndexEntry&& ie ) -> IndexEntry&
  {
    uLower = ie.uLower;
    uUpper = ie.uUpper;
    pIndex = std::move( ie.pIndex );
    return *this;
  }

  auto  IndexLayers::IndexEntry::Override( mtc::api<const IEntity> entity ) const -> mtc::api<const IEntity>
  {
    return uLower > 1 ? Override::Entity( entity ).Index(
      entity->GetIndex() + uLower - 1 ) : entity;
  }

  // IndexLayers::Entities implementation

  IndexLayers::Entities::Entities( const mtc::Iface* pix ):
    holder( pix ), pblock( blocks.begin() )
  {
  }

  void  IndexLayers::Entities::AddBlock( const BlockEntry& block )
  {
    if ( blocks.empty() )
      bktype = block.entSet->Type();

    blocks.emplace_back( block );
      pblock = blocks.begin();
      ncount += block.entSet->Size();
  }

  auto  IndexLayers::Entities::Find( uint32_t ix ) -> Reference
  {
    for ( auto  getRef = Reference(); pblock != blocks.end(); ++pblock )
    {
      if ( ix < pblock->uLower )
        ix = pblock->uLower;

      while ( pblock != blocks.end() && pblock->uUpper < ix )
        ++pblock;

      if ( pblock == blocks.end() )
        break;

      if ( (getRef = pblock->entSet->Find( ix - pblock->uLower + 1 )).uEntity != (uint32_t)-1 )
        return getRef.uEntity += pblock->uLower - 1, getRef;
    }
    return { uint32_t(-1), {} };
  }

}}
