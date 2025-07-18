# include "contents-index-merger.hpp"
# include "dynamic-entities.hpp"
# include <mtc/radix-tree.hpp>
# include <stdexcept>

namespace DelphiX {
namespace indexer {
namespace fusion {

  using IEntityIterator = IContentsIndex::IEntityIterator;
  using IRecordIterator = IContentsIndex::IRecordIterator;
  using EntityReference = IContentsIndex::IEntities::Reference;

  class EntityIterator
  {
    mtc::api<IEntityIterator> iterator;
    mtc::api<const IEntity>   curValue;
    EntityId                  entityId;

  public:
    EntityIterator( const mtc::api<IContentsIndex>& contentsIndex ):
      iterator( contentsIndex->GetEntityIterator( "" ) ),
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

  class RecordIterator
  {
    mtc::api<IRecordIterator> iterator;
    std::string               curValue;

  public:
    RecordIterator( const mtc::api<IContentsIndex>& contentsIndex ):
      iterator( contentsIndex->GetRecordIterator() ),
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
        if ( (mapped = (*block.mapEntities)[entry.uEntity]) != uint32_t(-1) )
        {
          if ( buffer.size() == buffer.capacity() )
            buffer.reserve( buffer.capacity() + 0x10000 );

          buffer.push_back( { mapped, entry.details } );
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
    auto  outStream = storage->Entities();
    auto  selectSet = std::vector<size_t>( indices.size() );

  // create iterators list
    for ( auto& next: indices )
      iterators.emplace_back( next );

  // set zero document
    if ( Entity( std::allocator<char>(), uint32_t(-1) ).Serialize( outStream.ptr() ) == nullptr )
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
        if ( Entity( std::allocator<char>(),
          iterators[iFresh].Curr(),
          entityId,
          iterators[iFresh]->GetVersion(),
          iterators[iFresh]->GetExtra(), nullptr ).Serialize( outStream.ptr() ) == nullptr )
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
    auto  chains    = storage->Chains();
    auto  iterators = std::vector<RecordIterator>();
    auto  selectSet = std::vector<size_t>( indices.size() );
    auto  refVector = std::vector<EntityReference>( 0x100000 );
    auto  rootIndex = mtc::radix::tree<RadixLink>();
    auto  keyRecord = RadixLink{ 0, 0, 0, 0 };

  // create iterators list
    for ( auto& next : indices )
      iterators.emplace_back( next );

  // list all the keys and select merge lists
    for ( ; ; )
    {
      size_t              nCount = 0;
      const std::string*  select = nullptr;

    // select lower key
      for ( size_t i = 0; i != iterators.size(); ++i )
        if ( !iterators[i].Curr().empty() )
        {
          if ( nCount == 0 || select->compare( iterators[i].Curr() ) > 0 )
          {
            select = &iterators[selectSet[(nCount = 1), 0]].Curr();
          }
            else
          if ( select->compare( iterators[i].Curr() ) == 0 )
          {
            selectSet[nCount++] = i;
          }
        }

    // check if key is available
      if ( nCount != 0 )
      {
        auto  blockList = std::vector<MapEntities>( nCount );
        auto  buildStats = std::pair<uint32_t, uint32_t>{};

        for ( size_t i = 0; i != nCount; ++i )
          blockList[i] = { indices[selectSet[i]]->GetKeyBlock( *select ), &remapId[selectSet[i]] };

        refVector.resize( 0 );

        if ( (buildStats = MergeChains( chains, refVector, blockList )).second != 0 )
        {
          keyRecord.bkType = blockList.front().entityBlock->Type();
          keyRecord.uCount = buildStats.first;
          keyRecord.length = buildStats.second;

          rootIndex.Insert( *select, keyRecord );

          keyRecord.offset += buildStats.second;
        }

        for ( size_t i = 0; i != nCount; ++i )
          iterators[selectSet[i]].Next();
      } else break;
    }

    rootIndex.Serialize( contents.ptr() );
  }

  auto  ContentsMerger::Add( mtc::api<IContentsIndex> index ) -> ContentsMerger&
  {
    indices.emplace_back( index );
    remapId.emplace_back( index->GetMaxIndex() + 1 );
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
