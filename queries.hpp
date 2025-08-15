# if !defined( __DelphiX_queries_hpp__ )
# define __DelphiX_queries_hpp__
# include <mtc/interfaces.h>
# include "slices.hpp"

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
      using Limits = struct
      {
        unsigned uMin;
        unsigned uMax;
      };
      using Spread = struct
      {
        const EntryPos* pbeg;
        const EntryPos* pend;

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
      const EntrySet*  beg = nullptr;
      const EntrySet*  end = nullptr;

      bool    empty() const {  return beg == end;  }
      size_t  size() const  {  return end - beg;  }
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

    unsigned    dwMode = None;  
    unsigned    nWords = 0;     // общее количество слов в документе
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

}}

# endif   // !__DelphiX_queries_hpp__
