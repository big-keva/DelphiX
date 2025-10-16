# if !defined( __DelphiX_queries_hpp__ )
# define __DelphiX_queries_hpp__
# include <mtc/interfaces.h>
# include <initializer_list>
# include <mtc/arena.hpp>
# include <memory>

namespace DelphiX {
namespace queries {

  struct Abstract
  {
    struct BM25Term
    {
      unsigned  termId;
      double    dblIDF;      // format zone weight
      unsigned  format;      // document format zone
      unsigned  occurs;      // term occurences
    };

    struct EntryPos
    {
      unsigned  termID;
      unsigned  offset;
    };

    struct EntrySet
    {
      struct Limits
      {
        unsigned uMin;
        unsigned uMax;
      };
      struct Spread
      {
        const EntryPos* pbeg;
        const EntryPos* pend;

        auto    begin() const -> const EntryPos*  {  return pbeg;  }
        auto    end() const -> const EntryPos*  {  return pend;  }
        bool    empty() const {  return pbeg == pend;  }
        size_t  size() const {  return pend - pbeg;  }
      };

      Limits  limits;
      double  weight;
      double  center;
      Spread  spread;
    };

    struct Entries
    {
      const EntrySet*  pbeg = nullptr;
      const EntrySet*  pend = nullptr;

      auto    begin() const -> const EntrySet*  {  return pbeg;  }
      auto    end() const -> const EntrySet*  {  return pend;  }
      bool    empty() const {  return pbeg == pend;  }
      size_t  size() const  {  return pend - pbeg;  }
    };

    struct Factors
    {
      const BM25Term*  beg = nullptr;
      const BM25Term*  end = nullptr;

      bool    empty() const {  return beg == end;  }
      size_t  size() const  {  return end - beg;  }
    };

    enum: unsigned
    {
      None = 0,
      BM25 = 1,
      Rich = 2
    };

    unsigned  dwMode = None;
    unsigned  nWords = 0;     // общее количество слов в документе
    union
    {
      Entries entries;
      Factors factors;
    };
  };

  /*
  * IQuery
  *
  * Представление вычислителя поисковых запросов.
  *
  * Находит идентификаторы документов и возвращает структуры данных, годные для ранжирования.
  *
  * Временем жизни этих структур управляет вычислитель запросов, так что после перехода
  * к следующему документу данные структуры окажутся некорректными.
  */
  struct IQuery: mtc::Iface
  {
    virtual uint32_t  SearchDoc( uint32_t ) = 0;
    virtual Abstract  GetTuples( uint32_t ) = 0;
  };

  auto  GetQuotation( const Abstract& ) -> Abstract::Entries;

  auto  MakeAbstract( mtc::Arena&, const std::initializer_list<Abstract::EntrySet>& ) -> Abstract;
  auto  MakeEntrySet( mtc::Arena&, const std::initializer_list<Abstract::EntryPos>&, double = 0.1 ) -> Abstract::EntrySet;
  auto  MakeEntrySet( mtc::Arena&, const std::initializer_list<unsigned>&, double = 0.1 ) -> Abstract::EntrySet;

}}

# endif   // !__DelphiX_queries_hpp__
