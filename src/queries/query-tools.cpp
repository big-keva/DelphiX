# include "query-tools.hpp"
# include <moonycode/codes.h>
# include <stdexcept>

namespace DelphiX {
namespace queries {

  // Operator implementation

  Operator::Operator( const std::string& cmd, const mtc::zval& var ):
    command( cmd ),
    zparams( var )  {}

  auto  Operator::GetVector() const -> const mtc::array_zval&
  {
    if ( zparams.get_type() != mtc::zval::z_array_zval )
      throw std::invalid_argument( "operator value has to point to array" );
    return *zparams.get_array_zval();
  }

  auto  Operator::GetString() const -> const mtc::widestr
  {
    if ( zparams.get_type() == mtc::zval::z_charstr )
      return codepages::mbcstowide( codepages::codepage_utf8, *zparams.get_charstr() );
    if ( zparams.get_type() == mtc::zval::z_widestr )
      return *zparams.get_widestr();
    throw std::invalid_argument( "operator has to handle string" );
  }

  auto  Operator::GetStruct() const -> const mtc::zmap&
  {
    if ( zparams.get_type() != mtc::zval::z_zmap )
      throw std::invalid_argument( "operator value has to point to structure" );
    return *zparams.get_zmap();
  }

  Operator::operator const char *() const
  {
    return command.c_str();
  }

  bool  Operator::operator == ( const char* str ) const
  {
    return command == str;
  }

  //

  Operator  GetOperator( const mtc::zval& query )
  {
    switch ( query.get_type() )
    {
      case mtc::zval::z_charstr:
      case mtc::zval::z_widestr:
        return { "word", query };
      case mtc::zval::z_zmap:
      {
        auto& zmap = *query.get_zmap();
        auto  pbeg = zmap.begin();
        auto  xend = zmap.begin();

        if ( pbeg == zmap.end() )
          throw std::invalid_argument( "invalid (empty) query passed" );
        if ( ++xend != zmap.end() )
          throw std::invalid_argument( "query has more than one root entry" );
        if ( !pbeg->first.is_charstr() )
          throw std::invalid_argument( "query operator key has to be string" );
        return { pbeg->first.to_charstr(), pbeg->second };
      }
      default:
        throw std::invalid_argument( "invalid query type" );
    }
  }

}}