# include "posix-fs-dump-store.hpp"
# include <mtc/recursive_shared_mutex.hpp>
# include <mtc/byteBuffer.h>

namespace DelphiX {
namespace storage {
namespace posixFS {

  class DumpStore final: public IStorage::IDumpStore
  {
    implement_lifetime_control

  public:
    DumpStore( const mtc::api<mtc::IFlatStream>& fl ): file( fl ) {}

    auto  Get( int64_t ) const -> mtc::api<const mtc::IByteBuffer> override;
    auto  Put( const void*, size_t ) -> int64_t override;

  protected:
    mtc::api<mtc::IFlatStream>  file;
    std::mutex                  lock;

  };

  auto  CreateDumpStore( const mtc::api<mtc::IFlatStream>& st ) -> mtc::api<IStorage::IDumpStore>
  {
    return st != nullptr ? new DumpStore( st ) : nullptr;
  }

  // DumpStore implementation

  auto  DumpStore::Get( int64_t po ) const -> mtc::api<const mtc::IByteBuffer>
  {
    char  blkbuf[0x1000];
    auto  cbread = file->PGet( blkbuf, po, sizeof(blkbuf) );

    if ( cbread >= 10 )
    {
      size_t  buflen;
      auto    bufptr = ::FetchFrom( const_cast<const char*>( blkbuf ), buflen );
      auto    getbuf = mtc::CreateByteBuffer( buflen, mtc::enable_exceptions );
      size_t  cbcopy;

      memcpy( (void*)getbuf->GetPtr(), bufptr, cbcopy = std::min( size_t(cbread - (bufptr - blkbuf)), buflen ) );

      if ( cbcopy < buflen )
        file->PGet( cbcopy + (char*)getbuf->GetPtr(), po + cbread, buflen - cbcopy );

      return getbuf.ptr();
    }
    return nullptr;
  }

  auto  DumpStore::Put( const void* pv, size_t cb ) -> int64_t
  {
    auto  exlock = mtc::make_unique_lock( lock );
    auto  putpos = file->Size();

    ::Serialize(
    ::Serialize( file.ptr(), cb ), pv, cb );

    return putpos;
  }

}}}
