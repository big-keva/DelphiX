# if !defined( __DelphiX_contents_hpp__ )
# define __DelphiX_contents_hpp__
# include <mtc/iStream.h>
# include <mtc/iBuffer.h>
# include <functional>
# include "mtc/span.hpp"

namespace DelphiX
{
  struct IEntity;             // common object properties
  struct IStorage;            // data collections interface
  struct IContents;           // indexable entity properties interface

  class EntityId: public std::string_view, protected mtc::api<const mtc::Iface>
  {
    using std::string_view::string_view;

  public:
    EntityId( const EntityId& ) = default;
    EntityId( const std::string_view& s, api i = nullptr ): std::string_view( s ), api( i ) {}
    EntityId( const std::string& s, api i = nullptr ): std::string_view( s.data(), s.size() ), api( i ) {}
  };

  struct IEntity: mtc::Iface
  {
    virtual auto  GetId() const -> EntityId = 0;
    virtual auto  GetIndex() const -> uint32_t = 0;
    virtual auto  GetExtra() const -> mtc::api<const mtc::IByteBuffer> = 0;
    virtual auto  GetBundle() const -> mtc::api<const mtc::IByteBuffer> = 0;
    virtual auto  GetVersion() const -> uint64_t = 0;
  };

  struct IStorage: mtc::Iface
  {
    struct IIndexStore;       // interface to write indices
    struct ISerialized;       // interface to read indices
    struct ISourceList;
    struct IDumpStore;

    virtual auto  ListIndices() -> mtc::api<ISourceList> = 0;
    virtual auto  CreateStore() -> mtc::api<IIndexStore> = 0;
  };

  struct IStorage::IIndexStore: Iface
  {
    virtual auto  Entities() -> mtc::api<mtc::IByteStream> = 0;
    virtual auto  Contents() -> mtc::api<mtc::IByteStream> = 0;
    virtual auto  Linkages() -> mtc::api<mtc::IByteStream> = 0;
    virtual auto  Packages() -> mtc::api<IDumpStore> = 0;

    virtual auto  Commit() -> mtc::api<ISerialized> = 0;
    virtual void  Remove() = 0;
  };

  struct IStorage::ISerialized: Iface
  {
    struct IPatch;

    virtual auto  Entities() -> mtc::api<const mtc::IByteBuffer> = 0;
    virtual auto  Contents() -> mtc::api<const mtc::IByteBuffer> = 0;
    virtual auto  Linkages() -> mtc::api<mtc::IFlatStream> = 0;
    virtual auto  Packages() -> mtc::api<IDumpStore> = 0;

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

  struct IStorage::ISourceList: Iface
  {
    virtual auto  Get() -> mtc::api<ISerialized> = 0;
  };

  struct IStorage::IDumpStore: Iface
  {
    virtual auto  Get( int64_t ) const -> mtc::api<const mtc::IByteBuffer> = 0;
    virtual auto  Put( const void*, size_t ) -> int64_t = 0;
  };

  struct IContentsIndex: mtc::Iface
  {
    struct IEntities;
    struct IIndexAPI;
    struct IEntitiesList;
    struct IContentsList;

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
      mtc::api<const IContents> keys = {},
      const std::string_view&   xtra = {},
      const std::string_view&   beef = {} ) -> mtc::api<const IEntity> = 0;

   /*
    * SetExtras()
    *
    * Changes the value of extras block for the entity identified by id
    */
    virtual auto  SetExtras( EntityId, const std::string_view& ) -> mtc::api<const IEntity> = 0;

    /*
    * Index statistics and service information
    */
    virtual auto  GetMaxIndex() const -> uint32_t = 0;

   /*
    * Blocks search api
    */
    virtual auto  GetKeyBlock( const std::string_view& ) const -> mtc::api<IEntities> = 0;
    virtual auto  GetKeyStats( const std::string_view& ) const -> BlockInfo = 0;

   /*
    * Iterators
    */
    virtual auto  ListEntities( EntityId ) -> mtc::api<IEntitiesList> = 0;
    virtual auto  ListEntities( uint32_t ) -> mtc::api<IEntitiesList> = 0;

    virtual auto  ListContents( const std::string_view& = {} ) -> mtc::api<IContentsList> = 0;
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
    * Remove()
    *
    * Forwards remove call to the nested storage manager
    */
    virtual void  Remove() = 0;

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
    virtual void  Insert( const std::string_view& key, const std::string_view& block, unsigned bkType ) = 0;
  };

  struct IContentsIndex::IEntities: Iface
  {
    struct Reference
    {
      uint32_t          uEntity;
      std::string_view  details;
    };

    virtual auto  Find( uint32_t ) -> Reference = 0;
    virtual auto  Size() const -> uint32_t = 0;
    virtual auto  Type() const -> uint32_t = 0;
  };

  struct IContentsIndex::IEntitiesList: Iface
  {
    virtual auto  Curr() -> mtc::api<const IEntity> = 0;
    virtual auto  Next() -> mtc::api<const IEntity> = 0;
  };

  struct IContentsIndex::IContentsList: Iface
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
            void  List( std::function<void(const std::string_view&, const std::string_view&, unsigned)> );
  };

  inline
  auto  make_view( const mtc::IByteBuffer* buffer ) -> std::string_view
  {
    return buffer != nullptr ? std::string_view( buffer->GetPtr(), buffer->GetLen() ) : std::string_view();
  }

}

# endif   // __DelphiX_contents_hpp__
