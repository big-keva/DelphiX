# if !defined( __DelphiX_enquote_quotations_hpp__ )
# define __DelphiX_enquote_quotations_hpp__
# include "DeliriX/text-API.hpp"
# include "../fields.hpp"
# include "../queries.hpp"
# include <functional>

namespace DelphiX {
namespace enquote {

  using QuotesFunc = std::function<void( DeliriX::IText*,
    const mtc::span<const char>&,
    const mtc::span<const char>&,
    const queries::Abstract&)>;

  class QuoteMachine
  {
    class common_settings;
    class quoter_function;

  public:
    QuoteMachine( const FieldHandler& );
    QuoteMachine( const QuoteMachine& ) = default;

  public:
    auto  SetLabels( const char* open, const char* close ) -> QuoteMachine&;
    auto  SetIndent( const FieldOptions::indentation& ) -> QuoteMachine&;

  public:
    auto  Structured() -> QuotesFunc;
    auto  TextSource() -> QuotesFunc;

  protected:
    std::shared_ptr<common_settings>  settings;

  };

}}

# endif   // !__DelphiX_enquote_quotations_hpp__
