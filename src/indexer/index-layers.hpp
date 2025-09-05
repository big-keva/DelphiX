# if !defined( __DelphiX_src_indexer_index_layers_hpp__ )
# define __DelphiX_src_indexer_index_layers_hpp__
#include <shared_mutex>

# include "../../contents.hpp"
# include "dynamic-bitmap.hpp"

namespace DelphiX {
namespace indexer {

 /*
  * IndexLayers обеспечивает хранение фиксированного массива элементов
  * с индекесами и базовые примитивы работы с ними.
  *
  * Сами индексы хранятся как атомарные переменные и работа с ними
  * ведётся на уровне забрать-положить.
  *
  * Это даёт возможность подменять интерфейсы индексов на лету (commit,
  * merge), а также добавлять новые до достижения предела размерности.
  *
  * Ротацию таких массивов при переполнении будет обеспечивать другой
  * компонент.
  */
  class IndexLayers
  {
    class Entities;

  public:
    IndexLayers() = default;
    IndexLayers( const mtc::api<IContentsIndex>*, size_t );
    IndexLayers( const std::vector<mtc::api<IContentsIndex>>& ppi ):
      IndexLayers( ppi.data(), ppi.size() ) {}

    auto  getEntity( EntityId ) const -> mtc::api<const IEntity>;
    auto  getEntity( uint32_t ) const -> mtc::api<const IEntity>;
    bool  delEntity( EntityId id );
    auto  setExtras( EntityId, const StrView& ) -> mtc::api<const IEntity>;

    auto  getMaxIndex() const -> uint32_t;
    auto  getKeyBlock( const StrView&, const mtc::Iface* = nullptr ) const -> mtc::api<IContentsIndex::IEntities>;
    auto  getKeyStats( const StrView& ) const -> IContentsIndex::BlockInfo;

    auto  listContents( const StrView&, const mtc::Iface* = nullptr ) -> mtc::api<IContentsIndex::IContentsList>;

    void  addContents( mtc::api<IContentsIndex> pindex );

    void  commitItems();

    void  hideClashes();

  protected:
    struct IndexEntry
    {
      uint32_t                  uLower;
      uint32_t                  uUpper;
      mtc::api<IContentsIndex>  pIndex;
      std::vector<IndexEntry>   backup;
      uint32_t                  dwSets = 0;

    public:
      IndexEntry( uint32_t uLower, mtc::api<IContentsIndex> pindex );
      IndexEntry( const IndexEntry& );
      IndexEntry& operator=( const IndexEntry& );

    public:
      auto  Override( mtc::api<const IEntity> ) const -> mtc::api<const IEntity>;

    };

    class ContentsList;

  protected:
    std::vector<IndexEntry> layers;

  };

  class IndexLayers::ContentsList final: public IContentsIndex::IContentsList
  {
    implement_lifetime_control

    ContentsList( const std::vector<mtc::api<IContentsList>>&, const Iface* );

  public:
    auto  Curr() -> std::string override;
    auto  Next() -> std::string override;

  protected:
    struct Iterator: protected mtc::api<IContentsList>
    {
      using api::api;

      std::string contentsItem;

      Iterator( const api& list ): api( list ), contentsItem( list->Curr() )  {}

    public:
      auto  Curr() -> const std::string&  {  return contentsItem;  }
      auto  Next() -> const std::string&  {  return contentsItem = ptr()->Next();  }
    };

    mtc::api<const Iface> parentObject;
    std::vector<Iterator> contentsList;
    const std::string*    currentValue = nullptr;


  };

}}

# endif   // !__DelphiX_src_indexer_index_layers_hpp__
