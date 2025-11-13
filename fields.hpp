# if !defined( __DelphiX_fields_hpp__ )
# define __DelphiX_fields_hpp__
# include <string_view>

namespace DelphiX {

  struct FieldOptions
  {
    struct indentation
    {
      struct { unsigned min; unsigned max; }  lower;
      struct { unsigned min; unsigned max; }  upper;
    };

    enum: unsigned
    {
      ofNoBreakWords = 0x00000001,
      ofKeywordsOnly = 0x00000002,
      ofDisableIndex = 0x00010000,
      ofDisableQuote = 0x00020000,
      ofEnforceQuote = 0x00040000
    };

    unsigned          id;
    std::string_view  name;
    double            weight = 1.0;
    unsigned          options = 0;
    indentation       indents = default_indents;

    static constexpr indentation default_indents = { { 2, 8 }, { 2, 8 } };
  };

  struct FieldHandler
  {
    virtual auto  Add( const std::string_view& ) ->             FieldOptions* = 0;
    virtual auto  Get( const std::string_view& ) const -> const FieldOptions* = 0;
    virtual auto  Get( unsigned       ) const -> const FieldOptions* = 0;
  };

}

# endif   // !__DelphiX_fields_hpp__
