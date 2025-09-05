# if !defined( __DelphiX_src_queries_decompressor_hpp__ )
# define __DelphiX_src_queries_decompressor_hpp__

namespace DelphiX {
namespace queries {

  using Abstract = IQuery::Abstract;
  using EntrySet = Abstract::EntrySet;
  using EntryPos = EntrySet::EntryPos;

  struct ZeroForm
    {  const char* operator()( const char* src, uint8_t& fid ) const {  return fid = 0xff, src;  }  };

  struct LoadForm
    {  const char* operator()( const char* src, uint8_t& fid ) const {  return ::FetchFrom( src, fid );  }  };

  template <class GetFormId, size_t N, size_t M, class RankEntry>
  auto  UnpackEntries(
    EntrySet (&output)[N],
    EntryPos (&appear)[M], const StrView& source, const RankEntry& ranker, unsigned id ) -> unsigned
  {
    auto  outPtr( output );
    auto  outEnd( output + std::min( N, M ) );
    auto  appPtr( appear );
    auto  srcPtr( source.data() );
    auto  srcEnd( source.data() + source.size() );
    auto  uEntry = unsigned(0);

    while ( srcPtr < srcEnd && outPtr != outEnd )
    {
      uint8_t   formid;
      unsigned  uOrder;
      double    weight;

      srcPtr = GetFormId()( ::FetchFrom( srcPtr, uOrder ), formid );

      if ( (weight = ranker( uEntry = (uOrder += uEntry), formid )) < 0 )
        continue;

      *outPtr++ = { { uOrder, uOrder }, weight, double(uOrder), {  appear, appear + 1 } };
      *appPtr++ = { id, uEntry++ };
    }

    return unsigned(outPtr - output);
  }
/*
  template <class GetFormId, class RankEntry>
  auto  UnpackEntries(
    EntrySet* output,
    EntryPos* appear, const StrView& source, const RankEntry& ranker, unsigned id ) -> unsigned
  {
    auto  outPtr( output );
    auto  srcPtr( source.data() );
    auto  srcEnd( source.data() + source.size() );
    auto  uEntry = unsigned(0);

    while ( srcPtr < srcEnd )
    {
      uint8_t   formid;
      unsigned  uOrder;
      double    weight;

      srcPtr = GetFormId()( ::FetchFrom( srcPtr, uOrder ), formid );

      if ( (weight = ranker( uEntry = (uOrder += uEntry), formid )) < 0 )
        continue;

      *outPtr++ = { { uOrder, uOrder }, weight, double(uOrder), { appear, 1 } };
      *appear++ = { id, uEntry++ };
    }

    return unsigned(outPtr - output);
  }
*/

}}

# endif   // !__DelphiX_src_queries_decompressor_hpp__
