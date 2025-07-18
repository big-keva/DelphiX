# if !defined( __DelphiX_src_indexer_override_entities_hxx__ )
# define __DelphiX_src_indexer_override_entities_hxx__
# include "contents.hpp"
# include "dynamic-bitmap.hpp"

namespace DelphiX {
namespace indexer {

  struct Override final
  {
    class Entity;
    class Entities;
  };

  class Override::Entity
  {
    mtc::api<const IEntity> entity;

  public:
    Entity( mtc::api<const IEntity> entity ) : entity( entity ) {}

    auto  Index( uint32_t ) -> mtc::api<const IEntity>;
    auto  Extra( const mtc::api<const mtc::IByteBuffer>& ) -> mtc::api<const IEntity>;
  };

  class Override::Entities final: public IContentsIndex::IEntities
  {
    mtc::api<IContentsIndex::IEntities> entities;
    const Bitmap<std::allocator<char>>& suppress;
    mtc::api<const mtc::Iface>          lifetime;

    implement_lifetime_control

  public:
    Entities( mtc::api<IContentsIndex::IEntities>, const Bitmap<std::allocator<char>>&, const mtc::Iface* );

  public:
    auto  Find( uint32_t ) -> Reference override;
    auto  Size() const -> uint32_t override;
    auto  Type() const -> uint32_t override;

  };

}}

# endif   // !__DelphiX_src_indexer_override_entities_hxx__
