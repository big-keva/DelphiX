#include <contents.hpp>
#include <shared_mutex>
#include <mtc/recursive_shared_mutex.hpp>

namespace DelphiX {
namespace storage {
namespace posixFS {

  auto  CreateDumpStore( const mtc::api<mtc::IFlatStream>& ) -> mtc::api<IStorage::IDumpStore>;

}}}
