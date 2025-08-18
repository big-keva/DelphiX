# if !defined( __DelphiX_src_queries_field_set_hpp__ )
# define __DelphiX_src_queries_field_set_hpp__
# include "../../fields.hpp"

namespace DelphiX {
namespace queries {

  class FieldSet: public std::vector<unsigned>
  {
  public:
    FieldSet() = default;
    FieldSet( FieldSet&& ) = default;
    FieldSet( const FieldSet& ) = default;
    FieldSet& operator = ( FieldSet&& ) = default;
    FieldSet& operator = ( const FieldSet& ) = default;

    FieldSet( const FieldHandler& );
    FieldSet( const std::initializer_list<unsigned>& ufs )
      {  for ( auto& fdu: ufs )  AddField( fdu );  }
    FieldSet( const FieldHandler& fdh, const std::string& str )
      {  AddField( fdh.Get( str ) );  }
    FieldSet( const FieldHandler& fdh, const mtc::array_charstr& fds )
      {  for ( auto& str: fds ) AddField( fdh.Get( str ) );  }
    FieldSet& operator &= ( const FieldSet& );
    FieldSet& operator |= ( const FieldSet& );
    FieldSet  operator & ( const FieldSet& ) const;
    FieldSet  operator | ( const FieldSet& ) const;

    void  AddField( const std::initializer_list<unsigned>& );
    void  AddField( const FieldOptions* pf );
    void  AddField( unsigned );
  };

  inline
  FieldSet::FieldSet( const FieldHandler& fdh )
  {
    unsigned u = 0;

    for ( auto pf = fdh.Get( u ); pf != nullptr; pf = fdh.Get( ++u ) )
      AddField( pf );
  }

  inline
  FieldSet& FieldSet::operator &= ( const FieldSet& fds )
  {
    auto  mbeg = begin();

    for ( auto pbeg = fds.begin(); mbeg != end() && pbeg != fds.end(); ++pbeg )
    {
      while ( mbeg != end() && *mbeg < *pbeg )
        mbeg = erase( mbeg );
      if ( mbeg != end() && *mbeg == *pbeg )
        ++mbeg;
    }
    return resize( mbeg - begin() ), *this;
  }

  inline
  FieldSet& FieldSet::operator |= ( const FieldSet& fds )
  {
    auto  mbeg = begin();

    for ( auto pbeg = fds.begin(); pbeg != fds.end(); ++pbeg )
    {
      while ( mbeg != end() && *mbeg < *pbeg )
        ++mbeg;
      if ( mbeg == end() || *mbeg != *pbeg )
        mbeg = insert( mbeg, *pbeg );
    }
    return *this;
  }

  inline
  FieldSet  FieldSet::operator & ( const FieldSet& fds ) const
  {
    auto  fset( *this );

    return std::move( fset &= fds );
  }

  inline
  FieldSet  FieldSet::operator | ( const FieldSet& fds ) const
  {
    auto  fset( *this );

    return std::move( fset |= fds );
  }

  inline
  void  FieldSet::AddField( const std::initializer_list<unsigned>& fs )
  {
    for ( auto& next: fs )
      AddField( next );
  }

  inline
  void  FieldSet::AddField( const FieldOptions* pf )
  {
    return pf != nullptr ? AddField( pf->id ) : (void)NULL;
  }

  inline
  void  FieldSet::AddField( unsigned id )
  {
    auto  pf = std::lower_bound( begin(), end(), id );

    if ( pf == end() || *pf != id )
      insert( pf, id );
  }

}}

# endif   // !__DelphiX_src_queries_field_set_hpp__
