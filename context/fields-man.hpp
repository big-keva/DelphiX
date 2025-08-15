# if !defined( __DelphiX_context_fields_man_hpp__ )
# define __DelphiX_context_fields_man_hpp__
# include "../fields.hpp"
# include <mtc/zmap.h>
# include <memory>

namespace DelphiX {
namespace context {

  class FieldManager: public FieldHandler
  {
    struct impl;

    std::shared_ptr<impl> data;

    friend  FieldManager    LoadFields( const mtc::array_zmap& );     // throws invalid_argument
    friend  mtc::array_zmap SaveFields( const FieldManager& fields );

  public:
    auto  Add( const StrView& )       ->       FieldOptions* override;
    auto  Get( const StrView& ) const -> const FieldOptions* override;
    auto  Get( unsigned       ) const -> const FieldOptions* override;
  };

  auto  LoadFields( const mtc::zmap&, const mtc::zmap::key& ) -> FieldManager;     // throws invalid_argument
  auto  LoadFields( const mtc::array_zmap& ) -> FieldManager;     // throws invalid_argument
  auto  SaveFields( const FieldManager& ) -> mtc::array_zmap;

}}

# endif   // !__DelphiX_context_fields_man_hpp__
