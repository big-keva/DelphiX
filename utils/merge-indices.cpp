# include "src/indexer/contents-index-merger.hpp"
# include "indexer/static-contents.hpp"
# include "storage/posix-fs.hpp"
# include <vector>
# include <string>

using namespace DelphiX;

int   MergeIndices(
  mtc::api<IStorage::IIndexStore> output,
  const std::vector<mtc::api<IContentsIndex>>& sources )
{
  try
  {
    indexer::fusion::ContentsMerger()
      .Set( output )
      .Set( sources )();
    output->Commit();
    return 0;
  }
  catch ( ... )
  {
    return -1;
  }
}

int   MergeIndices(
  const std::string& output,
  const std::vector<std::string>& sources )
{
  mtc::api<IStorage::IIndexStore>        outSink;
  std::vector<mtc::api<IContentsIndex>>  indices;

  for ( auto& next: sources )
  {
    auto  srcSerialized = storage::posixFS::OpenSerial(
      storage::posixFS::StoragePolicies::OpenInstance( next ) );
    auto  contentsIndex = indexer::static_::Contents().Create( srcSerialized );

    indices.push_back( contentsIndex );
  }
  outSink = storage::posixFS::CreateSink(
    storage::posixFS::StoragePolicies::OpenInstance( output ) );

  return MergeIndices( outSink, indices );
}

const char about[] = "merge-indices - palmira index merger\n"
  "Usage: merge-indices generic-output-path generic-source-1 [generic-source-2 [...]]\n";

int main( int argc, char* argv[] )
{
  auto  output = std::string();
  auto  source = std::vector<std::string>();

// load merge parameters
  for ( auto i = 1; i < argc; ++i )
    if ( output.empty() ) output = argv[i];
      else source.emplace_back( argv[i] );

// check valid parameters
  if ( output.empty() || source.empty() )
    return fprintf( stdout, about ), 0;

// try merge indices
  return MergeIndices( output, source );
}
