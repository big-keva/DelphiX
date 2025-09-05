# if !defined( __DelphiX_slices_hpp__ )
# define __DelphiX_slices_hpp__
# include <mtc/iBuffer.h>
# include <algorithm>
# include <string_view>
# include <string>
#include <vector>

namespace DelphiX
{
  template <class T>
  class Slice
  {
  public:
    Slice() = default;
    Slice( const Slice& ) = default;
    Slice( T* data, size_t size ):
      items( data ), count( size ) {}
  template <class Iterable>
    Slice( Iterable& coll ):
      items( coll.data() ), count( coll.size() )  {}

  public:
    auto  begin() const -> T* {  return items;  }
    auto  end() const -> T*  {  return items + count;  }
    auto  cbegin() const -> T* {  return items;  }
    auto  cend() const -> T*  {  return items + count;  }
    auto  data() const -> T* {  return items;  }
    auto  size() const -> size_t   {  return count;  }
    bool  empty() const {  return count == 0;  }

    auto  at( size_t pos ) const -> T& {  return items[pos];  }
    auto  operator[]( size_t pos ) const -> T& {  return items[pos];  }

    auto  front() const -> T&  {  return *items;  }
    auto  back() const -> T&  {  return items[count - 1];  }

    int   compare( const Slice& ) const;

    bool  operator==( const Slice& to ) const  {  return compare( to ) == 0;  }
    bool  operator!=( const Slice& to ) const  {  return !(*this == to);  }
    bool  operator< ( const Slice& to ) const  {  return compare( to ) <  0;  }
    bool  operator<=( const Slice& to ) const  {  return compare( to ) <= 0;  }
    bool  operator> ( const Slice& to ) const  {  return compare( to ) >  0;  }
    bool  operator>=( const Slice& to ) const  {  return compare( to ) >= 0;  }

  protected:
    T*      items = nullptr;
    size_t  count = 0;

  };

  class StrView: public Slice<const char>
  {
    using Slice::Slice;

  public:
    StrView( mtc::api<const mtc::IByteBuffer> );
  template <class Allocator>
    StrView( const std::basic_string<char, std::char_traits<char>, Allocator>& src ): StrView( src.data(), src.size() ) {}
  template <class Allocator>
    StrView( const std::vector<char, Allocator>& src ): StrView( src.data(), src.size() ) {}
  template <class ViewBytes>
    StrView( const ViewBytes& src ): StrView( src.data(), src.size() ) {}
    StrView( const char* pch );

    bool  operator==( const StrView& to ) const  {  return compare( to ) == 0;  }
    bool  operator!=( const StrView& to ) const  {  return !(*this == to);  }
    bool  operator< ( const StrView& to ) const  {  return compare( to ) <  0;  }
    bool  operator<=( const StrView& to ) const  {  return compare( to ) <= 0;  }
    bool  operator> ( const StrView& to ) const  {  return compare( to ) >  0;  }
    bool  operator>=( const StrView& to ) const  {  return compare( to ) >= 0;  }

    int  compare( const StrView& rhs ) const
      {  return ((Slice<const unsigned char>*)this)->compare( *(Slice<const unsigned char>*)&rhs );  }

    operator std::string_view() const {  return { items, count };  }

  };

  // Slice template implementation

  template <class T>
  int   Slice<T>::compare( const Slice& to ) const
  {
    auto  my_beg = this->items;
    auto  to_beg = to.items;
    auto  my_end = my_beg + std::min( this->count, to.count );
    int   rescmp;

    for ( rescmp = (my_beg != nullptr) - (to_beg != nullptr); rescmp == 0 && my_beg != my_end; ++my_beg, ++to_beg )
      rescmp = *my_beg - *to_beg;

    return rescmp != 0 ? rescmp : this->count - to.count;
  }
}

# endif   // __DelphiX_slices_hpp__
