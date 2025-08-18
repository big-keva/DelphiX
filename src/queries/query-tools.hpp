# if !defined( __DelphiX_src_queries_query_tools_hpp__ )
# define __DelphiX_src_queries_query_tools_hpp__
# include <mtc/zmap.h>

namespace DelphiX {
namespace queries {

  class Operator
  {
    std::string       command;
    const mtc::zval&  zparams;

  public:
    Operator( const std::string&, const mtc::zval& );
    Operator( const Operator& ) = default;

    auto  GetVector() const -> const mtc::array_zval&;
    auto  GetString() const -> const mtc::widestr;
    auto  GetStruct() const -> const mtc::zmap&;

    operator const char*() const;
  template <class T>
    bool  operator != ( T t ) const {  return !(*this == t);  }
    bool  operator == ( const char* ) const;
  };

  Operator  GetOperator( const mtc::zval& );

}}

# endif   // !__DelphiX_src_queries_query_tools_hpp__
