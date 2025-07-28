# if !defined( __DelphiX_query_block_type_hpp__ )
# define __DelphiX_query_block_type_hpp__

namespace DelphiX {
namespace context {

  struct EntryBlockType final
  {
    enum: unsigned
    {
      None = 0,           // no coordinates data

      EntryCount = 1,     // simple serialized BM25 values

      EntryOrder = 11,    // diff-compressed entries preceded by length
      EntryForms = 12     // diff-compressed entries + form ids preceded by length

    };
  };

}}

# endif // !__DelphiX_query_block_type_hpp__
