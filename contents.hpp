# if !defined( __DelphiX_contents_hpp__ )
# define __DelphiX_contents_hpp__
# include "slices.hpp"
# include <mtc/iStream.h>
# include <mtc/iBuffer.h>
# include <functional>

namespace DelphiX
{
  struct IEntity;             // common object properties
  struct IStorage;            // data collections interface
  struct IContents;           // indexable entity properties interface

  class EntityId: public StrView, protected mtc::api<const mtc::Iface>
  {
    using StrView::StrView;

  public:
    EntityId( const EntityId& ) = default;
    EntityId( const StrView& s, api i = nullptr ): StrView( s ), api( i ) {}
  };

  struct IEntity: mtc::Iface
  {
    virtual auto  GetId() const -> EntityId = 0;
    virtual auto  GetIndex() const -> uint32_t = 0;
    virtual auto  GetExtra() const -> mtc::api<const mtc::IByteBuffer> = 0;
    virtual auto  GetVersion() const -> uint64_t = 0;
  };

  struct IStorage: mtc::Iface
  {
    struct IIndexStore;       // interface to write indices
    struct ISerialized;       // interface to read indices
    struct ISourceList;

    virtual auto  ListIndices() -> mtc::api<ISourceList> = 0;
    virtual auto  CreateStore() -> mtc::api<IIndexStore> = 0;
  };

  struct IStorage::ISourceList: Iface
  {
    virtual auto  Get() -> mtc::api<ISerialized> = 0;
  };

  struct IStorage::IIndexStore: Iface
  {
    virtual auto  Entities() -> mtc::api<mtc::IByteStream> = 0;
    virtual auto  Contents() -> mtc::api<mtc::IByteStream> = 0;
    virtual auto  Chains() -> mtc::api<mtc::IByteStream> = 0;

    virtual auto  Commit() -> mtc::api<ISerialized> = 0;
    virtual void  Remove() = 0;
  };

  struct IStorage::ISerialized: Iface
  {
    struct IPatch;

    virtual auto  Entities() -> mtc::api<const mtc::IByteBuffer> = 0;
    virtual auto  Contents() -> mtc::api<const mtc::IByteBuffer> = 0;
    virtual auto  Blocks() -> mtc::api<mtc::IFlatStream> = 0;

    virtual auto  Commit() -> mtc::api<ISerialized> = 0;
    virtual void  Remove() = 0;

    virtual auto  NewPatch() -> mtc::api<IPatch> = 0;
  };

  struct IStorage::ISerialized::IPatch: Iface
  {
    virtual void  Delete( EntityId ) = 0;
    virtual void  Update( EntityId, const void*, size_t ) = 0;
    virtual void  Commit() = 0;
  };

  struct IContentsIndex: mtc::Iface
  {
    struct IEntities;
    struct IIndexAPI;
    struct IEntityIterator;
    struct IRecordIterator;

   /*
    * entity details block statistics
    */
    struct BlockInfo
    {
      uint32_t    bkType;
      uint32_t    nCount;
    };

   /*
    * GetEntity()
    *
    * Provide access to entity by entiry id or index.
    */
    virtual auto  GetEntity( EntityId ) const -> mtc::api<const IEntity> = 0;
    virtual auto  GetEntity( uint32_t ) const -> mtc::api<const IEntity> = 0;

   /*
    * DelEntity()
    *
    * Remove entity by id.
    */
    virtual bool  DelEntity( EntityId id ) = 0;

   /*
    * SetEntity()
    *
    * Insert object with id to the index, sets dynamic untyped attributes
    * and indexable properties
    */
    virtual auto  SetEntity( EntityId,
      mtc::api<const IContents> = {}, const StrView& = {} ) -> mtc::api<const IEntity> = 0;

   /*
    * SetExtras()
    *
    * Changes the value of extras block for the entity identified by id
    */
    virtual auto  SetExtras( EntityId, const StrView& ) -> mtc::api<const IEntity> = 0;

    /*
    * Index statistics and service information
    */
    virtual auto  GetMaxIndex() const -> uint32_t = 0;
//    virtual auto  CountEntities() const -> uint32_t = 0;

   /*
    * Blocks search api
    */
    virtual auto  GetKeyBlock( const StrView& ) const -> mtc::api<IEntities> = 0;
    virtual auto  GetKeyStats( const StrView& ) const -> BlockInfo = 0;

   /*
    * Iterators
    */
    virtual auto  GetEntityIterator( EntityId ) -> mtc::api<IEntityIterator> = 0;
    virtual auto  GetEntityIterator( uint32_t ) -> mtc::api<IEntityIterator> = 0;

    virtual auto  GetRecordIterator( const StrView& = {} ) -> mtc::api<IRecordIterator> = 0;
   /*
    * Commit()
    *
    * Writes all index data to storage held inside and returns
    * serialized interface.
    */
    virtual auto  Commit() -> mtc::api<IStorage::ISerialized> = 0;

   /*
    * Reduce()
    *
    * Return pointer to simplified version of index being optimized.
    */
    virtual auto  Reduce() -> mtc::api<IContentsIndex> = 0;

   /*
    * Stash( id )
    *
    * Stashes entity wothout any modifications to index.
    *
    * Defined for static indices.
    */
    virtual void  Stash( EntityId ) = 0;
  };

 /*
  * IContentsIndex::IKeyValue
  *
  * Internal indexer interface to set key -> value pairs for object being indexed.
  */
  struct IContentsIndex::IIndexAPI
  {
    virtual void  Insert( const StrView& key, const StrView& block, unsigned bkType ) = 0;
  };

  struct IContentsIndex::IEntities: Iface
  {
    struct Reference
    {
      uint32_t    uEntity;
      StrView        details;
    };

    virtual auto  Find( uint32_t ) -> Reference = 0;
    virtual auto  Size() const -> uint32_t = 0;
    virtual auto  Type() const -> uint32_t = 0;
  };

  struct IContentsIndex::IEntityIterator: Iface
  {
    virtual auto  Curr() -> mtc::api<const IEntity> = 0;
    virtual auto  Next() -> mtc::api<const IEntity> = 0;
  };

  struct IContentsIndex::IRecordIterator: Iface
  {
    virtual auto  Curr() -> std::string = 0;
    virtual auto  Next() -> std::string = 0;
  };

 /*
  * IContents - keys indexing API provided to IContentsIndex::SetEntity()
  *
  * Method is called with single interface argument to send (key; value)
  * pairs to contents index
  */
  struct IContents: mtc::Iface
  {
    virtual void  Enum( IContentsIndex::IIndexAPI* ) const = 0;
            void  List( std::function<void(const StrView&, const StrView&, unsigned)> );
  };

}

# endif   // __DelphiX_contents_hpp__
