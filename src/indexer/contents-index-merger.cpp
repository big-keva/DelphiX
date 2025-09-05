# include "contents-index-merger.hpp"
# include "dynamic-entities.hpp"
# include <mtc/radix-tree.hpp>
# include <stdexcept>

namespace DelphiX {
namespace indexer {
namespace fusion {

  using IEntityIterator = IContentsIndex::IEntitiesList;
  using IRecordIterator = IContentsIndex::IContentsList;
  using EntityReference = IContentsIndex::IEntities::Reference;

  class EntityIterator
  {
    mtc::api<IEntityIterator> iterator;
    mtc::api<const IEntity>   curValue;
    EntityId                  entityId;

  public:
    EntityIterator( const mtc::api<IContentsIndex>& contentsIndex ):
      iterator( contentsIndex->ListEntities( "" ) ),
      curValue( iterator->Curr() ),
      entityId( curValue != nullptr ? curValue->GetId() : EntityId() ) {}

  public:
    auto  operator -> () const -> mtc::api<const IEntity>
      {  return curValue;  }
    auto  Curr() -> const EntityId&
      {  return entityId;  }
    auto  Next() -> const EntityId&
      {  return entityId = (curValue = iterator->Next()) != nullptr ? curValue->GetId() : EntityId();  }

  };

  class LexemeIterator
  {
    mtc::api<IRecordIterator> iterator;
    std::string               curValue;

  public:
    LexemeIterator( const mtc::api<IContentsIndex>& contentsIndex ):
      iterator( contentsIndex->ListContents() ),
      curValue( iterator->Curr() ) {}

  public:
    auto  Curr() const -> const std::string&
      {  return curValue;  }
    auto  Next() -> const std::string&
      {  return curValue = iterator->Next();  }

  };

  struct MapEntities
  {
    mtc::api<IContentsIndex::IEntities> entityBlock;
    const std::vector<uint32_t>*        mapEntities;
  };

  struct RadixLink
  {
    uint32_t  bkType;
    uint32_t  uCount;
    uint64_t  offset;
    uint32_t  length;

  public:
    auto  GetBufLen() const
    {
      return ::GetBufLen( bkType ) + ::GetBufLen( uCount )
           + ::GetBufLen( offset ) + ::GetBufLen( length );
    }
    template <class O>
    O*    Serialize( O* o ) const
    {
      return ::Serialize( ::Serialize( ::Serialize( ::Serialize( o,
        bkType ), uCount ), offset ), length );
    }

  };

  auto  MergeSimple(
    mtc::api<mtc::IByteStream>      output,
    std::vector<EntityReference>&   buffer,
    const std::vector<MapEntities>& blocks ) -> std::pair<uint32_t, uint32_t>
  {
    uint32_t  length = 0;
    uint32_t  uOldId = 0;

    for ( auto& block: blocks )
    {
      uint32_t  mapped;

      // list all the references in the block
      for ( auto entry = block.entityBlock->Find( 0 ); entry.uEntity != uint32_t(-1); entry = block.entityBlock->Find( 1 + entry.uEntity ) )
      {
        if ( (mapped = block.mapEntities->at( entry.uEntity )) != uint32_t(-1) )
        {
          if ( buffer.size() == buffer.capacity() )
            buffer.reserve( buffer.capacity() + 0x10000 );
          buffer.push_back( { mapped, entry.details } );
        }
      }
    }

    // check if any objects in a buffer, resort, serialize and return the length
    std::sort( buffer.begin(), buffer.end(), []( const EntityReference& a, const EntityReference& b )
      {  return a.uEntity < b.uEntity; } );

    for ( auto& reference: buffer )
    {
      auto  diffId = reference.uEntity - uOldId - 1;

      if ( ::Serialize( output.ptr(), diffId ) == nullptr )
        throw std::runtime_error( "Failed to serialize entities" );

      length += ::GetBufLen( diffId );
      uOldId = reference.uEntity;
    }

    return { buffer.size(), length };
  }

  auto  MergeChains(
    mtc::api<mtc::IByteStream>      output,
    std::vector<EntityReference>&   buffer,
    const std::vector<MapEntities>& blocks ) -> std::pair<uint32_t, uint32_t>
  {
    uint32_t  length = 0;
    uint32_t  uOldId = 0;

    for ( auto& block: blocks )
    {
      uint32_t  mapped;

      // list all the references in the block
      for ( auto entry = block.entityBlock->Find( 0 ); entry.uEntity != uint32_t(-1); entry = block.entityBlock->Find( 1 + entry.uEntity ) )
      {
        if ( (mapped = block.mapEntities->at( entry.uEntity )) != uint32_t(-1) )
        {
          if ( buffer.size() == buffer.capacity() )
            buffer.reserve( buffer.capacity() + 0x10000 );
          buffer.push_back( { mapped, entry.details } );
        }
      }
    }

    // check if any objects in a buffer, resort, serialize and return the length
    std::sort( buffer.begin(), buffer.end(), []( const EntityReference& a, const EntityReference& b )
      {  return a.uEntity < b.uEntity; } );

    for ( auto& reference: buffer )
    {
      auto  diffId = reference.uEntity - uOldId - 1;
      auto  nbytes = reference.details.size();

      if ( ::Serialize( ::Serialize( ::Serialize( output.ptr(),
        diffId ),
        nbytes ), reference.details.data(), nbytes )  == nullptr )
      {
        throw std::runtime_error( "Failed to serialize entities" );
      }

      length += ::GetBufLen( diffId ) + ::GetBufLen( nbytes ) + nbytes;
      uOldId = reference.uEntity;
    }

    return { buffer.size(), length };
  }

  void  ContentsMerger::MergeEntities()
  {
    using Entity = dynamic::EntityTable<std::allocator<char>>::Entity;

    auto  iterators = std::vector<EntityIterator>();
    auto  selectSet = std::vector<size_t>( indices.size() );
    auto  entityStm = storage->Entities();
    auto  bundleStm = storage->Packages();

  // create iterators list
    for ( auto& next: indices )
      iterators.emplace_back( next );

  // set zero document
    if ( Entity( std::allocator<char>() ).Serialize( entityStm.ptr() ) == nullptr )
      throw std::runtime_error( "Failed to serialize entities" );

    for ( auto entityId = uint32_t(1); ; )
    {
      auto  nCount = size_t(0);
      auto  iFresh = size_t(-1);

    // find entity with minimal id and select the one with bigger Version
      for ( size_t i = 0; i != iterators.size(); ++i )
      {
        if ( iterators[i].Curr().empty() )
          continue;

        if ( nCount == 0 || iterators[selectSet[nCount - 1]].Curr().compare( iterators[i].Curr() ) > 0 )
        {
          selectSet[(nCount = 1), 0] = iFresh = i;
        }
          else
        if ( iterators[selectSet[nCount - 1]].Curr().compare( iterators[i].Curr() ) == 0 )
        {
          selectSet[nCount++] = i;

          if ( iterators[i]->GetVersion() > iterators[iFresh]->GetVersion() )
            iFresh = i;
        }
      }

    // check if no more entities
      if ( nCount != 0 )
      {
        auto  bundlePos = int64_t(-1);
        auto  bundlePtr = mtc::api<const mtc::IByteBuffer>();

        if ( bundleStm != nullptr && (bundlePtr = iterators[iFresh]->GetBundle()) != nullptr )
          bundlePos = bundleStm->Put( bundlePtr->GetPtr(), bundlePtr->GetLen() );

        if ( bundlePos == -1 )
        {
          int i = 0;
        }

        if ( Entity( std::allocator<char>() )
          .SetId( iterators[iFresh].Curr() )
          .SetIndex( entityId )
          .SetExtra( iterators[iFresh]->GetExtra() )
          .SetPackPos( bundlePos )
          .SetVersion( iterators[iFresh]->GetVersion() ).Serialize( entityStm.ptr() ) == nullptr )
        {
          throw std::runtime_error( "Failed to serialize entities" );
        }

      // fill renumbering maps and request next documents
        for ( size_t i = 0; i != nCount; ++i )
        {
          if ( selectSet[i] == iFresh )
            remapId[selectSet[i]][iterators[selectSet[i]]->GetIndex()] = entityId++;
          else
            remapId[selectSet[i]][iterators[selectSet[i]]->GetIndex()] = uint32_t(-1);

          iterators[selectSet[i]].Next();
        }
      } else break;
    }
  }

  void  ContentsMerger::MergeContents()
  {
    auto  contents  = storage->Contents();
    auto  chains    = storage->Linkages();
    auto  iterators = std::vector<LexemeIterator>();
    auto  selectSet = std::vector<size_t>( indices.size() );
    auto  refVector = std::vector<EntityReference>( 0x100000 );
    auto  radixTree = mtc::radix::tree<RadixLink>();
    auto  keyRecord = RadixLink{ 0, 0, 0, 0 };

  // create iterators list
    for ( auto& next : indices )
      iterators.emplace_back( next );

  // list all the keys and select merge lists
    for ( ; ; )
    {
      auto  nCount = size_t(0);
      auto  select = (const std::string*)nullptr;

    // select lower key
      for ( size_t i = 0; i != iterators.size(); ++i )
        if ( !iterators[i].Curr().empty() )
        {
          int   rescmp;

          if ( nCount == 0 || (rescmp = select->compare( iterators[i].Curr() )) >= 0 )
          {
            if ( rescmp > 0 )
              nCount = 0;
            select = &iterators[selectSet[nCount++] = i].Curr();
          }
        }

    // check if key is available
      if ( nCount != 0 )
      {
        auto  blockList = std::vector<MapEntities>( nCount );
        auto  mergeStat = std::pair<uint32_t, uint32_t>{};

        for ( size_t i = 0; i != nCount; ++i )
          blockList[i] = { indices[selectSet[i]]->GetKeyBlock( *select ), &remapId[selectSet[i]] };

        refVector.resize( 0 );

        mergeStat = blockList.front().entityBlock->Type() == 0 ?
          MergeSimple( chains, refVector, blockList ) :
          MergeChains( chains, refVector, blockList );

        if ( mergeStat.second != 0 )
        {
          keyRecord.bkType = blockList.front().entityBlock->Type();
          keyRecord.uCount = mergeStat.first;
          keyRecord.length = mergeStat.second;

          radixTree.Insert( *select, keyRecord );

          keyRecord.offset += mergeStat.second;
        }

        for ( size_t i = 0; i != nCount; ++i )
          iterators[selectSet[i]].Next();
      } else break;
    }

    radixTree.Serialize( contents.ptr() );
  }

  auto  ContentsMerger::Add( mtc::api<IContentsIndex> index ) -> ContentsMerger&
  {
    indices.emplace_back( index );
    remapId.emplace_back( index->GetMaxIndex() + 2 );
    return *this;
  }

  auto  ContentsMerger::Set( std::function<bool()> can ) -> ContentsMerger&
  {
    canContinue = can != nullptr ? can : [](){  return true;  };
    return *this;
  }

  auto  ContentsMerger::Set( mtc::api<IStorage::IIndexStore> out ) -> ContentsMerger&
  {
    storage = out;
    return *this;
  }

  auto  ContentsMerger::Set( const mtc::api<IContentsIndex>* pi, size_t cc ) -> ContentsMerger&
  {
    for ( auto pe = pi + cc; pi != pe; ++pi )
      Add( *pi );
    return *this;
  }

  auto  ContentsMerger::Set( const std::vector<mtc::api<IContentsIndex>>& ix ) -> ContentsMerger&
  {
    for ( auto& next: ix )
      Add( next );
    return *this;
  }

  auto  ContentsMerger::Set( const std::initializer_list<const mtc::api<IContentsIndex>>& vx ) -> ContentsMerger&
  {
    for ( auto& next: vx )
      Add( next );
    return *this;
  }

  auto  ContentsMerger::operator()() -> mtc::api<IStorage::ISerialized>
  {
    MergeEntities();
    MergeContents();
    return storage->Commit();
  }

}}}
