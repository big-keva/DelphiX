# include "primes.hpp"

namespace DelphiX {

  auto  UpperPrime( size_t n ) -> size_t
  {
    for ( n += (n % 2) + 1; ; n += 2 )
    {
      auto  m = n / 2;

      while ( m > 2 && (n % m) != 0 )
        --m;

      if ( m == 2 )
        return n;
    }
  }

}
