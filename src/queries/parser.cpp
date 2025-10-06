# include "../../queries/parser.hpp"
# include "../../context/processor.hpp"

namespace DelphiX {
namespace queries {

  using Token = textAPI::TextToken;

  struct Operator
  {
    enum: unsigned
    {
      Unknown = 0,
      AND   = 1,
      OR    = 2,
      NOT   = 3,
    };

    unsigned  code = Unknown;
    unsigned  size = 0;

    auto  to_string() const -> const char*
      {
        return
          code == AND ? "&&" :
          code == OR  ? "||" :
          code == NOT ? "!"  : throw std::invalid_argument( "unexpected operator value" );
      }
  };

  struct Function
  {
    enum: unsigned
    {
      Unknown = 0,
      context = 1,
      cover   = 2,
      match   = 3
    };

    unsigned  code = Unknown;
    unsigned  size = 0;

    auto  to_string() const -> const char*
      {
        return
          code == context ? "context" :
          code == cover ? "cover" :
          code == match ? "match" : throw std::invalid_argument( "unexpected function value" );
      }
  };

  bool  IsChar( const Token& t, widechar c ) noexcept     {  return t.length == 1 && *t.pwsstr == c;  }
  bool  IsChar( const Token& t, char c ) noexcept         {  return t.length == 1 && IsChar( t, codepages::xlatWinToUtf16[(unsigned char)c] );  }
  bool  IsWord( const Token& t, const char* s ) noexcept  {  return t.length == strlen( s ) && mtc::w_strncasecmp( t.pwsstr, s, t.length ) == 0;  }

  bool  operator == ( const Token& t, char c )            {  return IsChar( t, c );  }
  bool  operator == ( const Token& t, const char* s )     {  return IsWord( t, s );  }
template <class T>
  bool  operator != ( const Token& t, T x )               {  return !(t == x);  }

  auto  GetOperator( const Token*, const Token* ) -> Operator;
  auto  GetFunction( const Token*, const Token* ) -> Function;
  auto  GetExpLen( const Token*, const Token* ) -> size_t;
  auto  ParseField( unsigned, const Token*, const Token* ) -> mtc::zval;
  auto  ParseContext( const Token*, const Token* ) -> mtc::zval;

  auto  GetOperator( const Token* beg, const Token* end ) -> Operator
  {
    if ( *beg == '&' )
      return { Operator::AND, ++beg != end && !beg->LeftSpaced() && *beg == '&' ? 2U : 1U };
    if ( *beg == '|' )
      return { Operator::OR,  ++beg != end && !beg->LeftSpaced() && *beg == '|' ? 2U : 1U };
    if ( *beg == '!' || *beg == '-' )
      return { Operator::NOT, 1 };
    if ( *beg == "and" )
      return { Operator::AND, 1 };
    if ( *beg == "or" )
      return { Operator::OR, 1 };
    if ( *beg == "not" )
      return { Operator::OR, 1 };
    return { Operator::Unknown, 0 };
  }

  auto  GetFunction( const Token* beg, const Token* end ) -> Function
  {
    if ( end - beg >= 3 && beg[1] == '(' && !beg[1].LeftSpaced() )
    {
      auto  arglen = GetExpLen( beg + 1, end );

      if ( *beg == "ctx" || *beg == "context" )
        return { Function::context, unsigned(1 + arglen) };
      if ( *beg == "cover" )
        return { Function::cover, unsigned(1 + arglen) };
      if ( *beg == "match" )
        return { Function::match, unsigned(1 + arglen) };
    }
    return { Function::Unknown, 0 };
  }

  auto  GetExpLen( const Token* beg, const Token* end ) -> size_t
  {
    size_t  len;
    bool    esc;

    if ( beg == end )
      return 0;

   /*
    * check if grouped with ()
    */
    if ( *beg == '(' )
    {
      auto    len = 1;
      auto    brc = 1;

      for ( auto  sub = GetExpLen( beg + len, end ); beg + len != end && brc > 0; sub = GetExpLen( beg + (len += sub), end ) )
        if ( sub == 1 )
        {
          if ( beg[len] == '(' )  ++brc;
            else
          if ( beg[len] == ')' )  --brc;
        }

      if ( brc != 0 )
        throw ParseError( "')' expected" );

      return len;
    }

   /*
    * check if quoted string
    */
    if ( *beg == '"' || *beg == '\'' )
    {
      auto  size = size_t(1);
      auto  stop = *beg->pwsstr;

      for ( auto escape = false; beg + size != end; )
      {
        if ( beg[size] == '\\' && !escape )  {  escape = true;  ++size;  }
          else
        if ( beg[size] == stop && !escape )  {  return size + 1;  }
          else
        {  escape = false;  ++size;  }
      }
      throw ParseError( mtc::strprintf( "'%c' expected", (char)*beg->pwsstr ) );
    }

   /*
    * check if function
    */
    for ( auto fn = GetFunction( beg, end ); fn.code != Function::Unknown; )
      return fn.size;

// check functions
//    if ( end - top > 1 && IsFunction( *top ) != func_unknown && top[1] == '(' )
//      return 1 + GetExprLen( top + 1, end );

   /*
    * check if operator
    */
    for ( auto op = GetOperator( beg, end ); op.code != Operator::Unknown; )
      return op.size;

   /*
    * else the expression length is either 1, or, if it is a wildcard expression,
    * a sequence; check if has not escaped * and ?
    */
    esc = IsChar( *beg, '\\' );

    for ( len = 1; beg + len != end && !beg[len].LeftSpaced(); ++len )
    {
      if ( beg[len].uFlags & textAPI::TextToken::is_punct )
      {
        if ( IsChar( beg[len], '\\' ) ) esc = !esc;
          else
        if ( IsChar( beg[len], '*' ) || IsChar( beg[len], '?' ) )
        {
          if ( esc )
            return len > 1 ? len - 1 : len + 1;
        }
          else
        return len;
      }
    }
    return len;
  }

  auto  CreateWord( const Token* ptrtop, const Token* ptrend ) -> mtc::zval
  {
    auto  mkword = mtc::widestr();
    auto  isText = true;

    for ( auto esc = false; ptrtop != ptrend; ++ptrtop )
    {
      if ( IsChar( *ptrtop, '\\' ) && !esc )  esc = true;
        else
      {
        isText &= esc || !(IsChar( *ptrtop, '*' ) || IsChar( *ptrtop, '?' ));
        mkword += { ptrtop->pwsstr, ptrtop->length };
      }
    }
    return !isText ? mtc::zmap{ { "wildcard", mkword } } : mtc::zval( mkword );
  }

  auto  ParseQuery( const Token* ptrtop, const Token* ptrend ) -> mtc::zval
  {
   /*
    * Снять возможные парные скобки
    */
    while ( ptrtop != ptrend
      && ptrtop[ 0] == '('
      && ptrend[-1] == ')'
      && GetExpLen( ptrtop, ptrend ) == size_t(ptrend - ptrtop) )
    {
      ++ptrtop;  --ptrend;
    }

    if ( ptrtop != ptrend )
    {
      auto    breaks = std::vector<std::pair<const Token*, const Token*>>();
      auto    subset = mtc::array_zval();
      auto    divide = Operator();
      bool    waitOp = false;
      size_t  explen;

     /*
      * Проверить одинарные и двойные кавычки для последовательности
      */
      if ( ptrend - ptrtop > 1
        && (ptrtop[0] == '"' || ptrtop[0] == '\'')
        && (ptrtop[0] == ptrend[-1])
        && GetExpLen( ptrtop, ptrend ) == size_t(ptrend - ptrtop) )
      {
        bool  forced = *ptrtop++ == '"';

        for ( --ptrend; ptrtop != ptrend; ++ptrtop )
          subset.emplace_back( mtc::widestr( ptrtop->pwsstr, ptrend->length ) );

        return mtc::zmap{
          { forced ? "quote" : "order", std::move( subset ) } };
      }

     /*
      * Разбить последовательность на подзапросы по самым 'слабым' операторам.
      *
      * Если получилось, разобрать каждую подпоследовательность отдельно.
      */
      for ( auto pwnext = ptrtop, pstart = ptrtop; pwnext != ptrend; pwnext += explen )
      {
        auto  op = Operator();

        if ( (explen = GetExpLen( pwnext, ptrend )) > 2 )
          {  waitOp = true;  continue;  }

        if ( (op = GetOperator( pwnext, ptrend )).size == 0 )
          {  waitOp = true;  continue;  }

        if ( !waitOp || pwnext + op.size == ptrend )
          throw ParseError( "unexpected operator" );

        if ( divide.code < op.code )
          {  breaks.clear();  pstart = ptrtop;  }

        if ( breaks.size() == 0 || op.code == divide.code )
          {  breaks.emplace_back( pstart, pwnext );  pstart = pwnext + (divide = op).size;  }

        waitOp = false;
      }

     /*
      * Если есть некоторое разбиение, дополнить его последним элементом, разобрать
      * выделеннные подзапросы и создать выбранный оператор
      */
      if ( breaks.size() != 0 )
      {
        breaks.emplace_back( breaks.back().second + divide.size, ptrend );

        for ( auto& next: breaks )
          subset.emplace_back( ParseQuery( next.first, next.second ) );

        return mtc::zmap{ { divide.to_string(), std::move( subset ) } };
      }

      /*
       * Проверить, не функция ли
       */
      for ( auto fn = GetFunction( ptrtop, ptrend ); fn.code != Function::Unknown && fn.size == ptrend - ptrtop; )
      {
        switch ( fn.code )
        {
          case Function::context:
            return ParseContext( ptrtop + 2, ptrend - 1 );
          case Function::cover:
          case Function::match:
            return ParseField( fn.code, ptrtop + 2, ptrend - 1 );
          default:  throw std::logic_error( "unsupported function" );
        }
      }

    /*
     * Разбить последовательность терминов, заранее зная, что как минимум первый в ряду - именно
     * термин, а не функция, не скобочная конструкция и не цитата
     *
     * Создать первый элемент в последовательном разбиении фразы без операторов и функций
     */
      subset.emplace_back( CreateWord( ptrtop, ptrtop + (explen = GetExpLen( ptrtop, ptrend )) ) );

      for ( auto pwnext = ptrtop + explen; pwnext != ptrend; pwnext += explen )
      {
        explen = GetExpLen( pwnext, ptrend );

        subset.emplace_back( ParseQuery( pwnext, pwnext + explen ) );
      }

     /*
      * Проверить, одиночный это термин или некотороая последовательность
      */
      if ( subset.size() == 0 )
        throw std::logic_error( "query parser algo fault @" __FILE__ ":" LINE_STRING );

      if ( subset.size() == 1 )
        return subset.front();

      return mtc::zmap{ { "fuzzy", std::move( subset ) } };
    }
    return {};
  }

  auto  ParseContext( const Token* ptrtop, const Token* ptrend ) -> mtc::zval
  {
    widechar* pwsend;
    unsigned  ulimit = mtc::w_strtoul( ptrtop->pwsstr, &pwsend, 0 );
    auto      zquery = mtc::zval();

  // check length and divider comma
    if ( ptrtop + 3 > ptrend || ptrtop[1] != ',' )
      throw ParseError( "context limit and query divided by comma expected as context limit @" __FILE__ ":" LINE_STRING );

  // get context length
    if ( ulimit == 0 || pwsend != ptrtop->pwsstr + ptrtop->length )
      throw ParseError( "integer context limit expected @" __FILE__ ":" LINE_STRING );

    return mtc::zmap{
      { "limit", mtc::zmap{
        { "context", ulimit },
        { "query", ParseQuery( ptrtop + 2, ptrend ) } } } };
  }

  auto  ParseField( unsigned fn, const Token* ptrtop, const Token* ptrend ) -> mtc::zval
  {
    mtc::array_charstr  fields;
    bool                waitOp;

  // check length and divider comma
    if ( ptrtop + 3 > ptrend )
      throw ParseError( "'fields..., query' expected" );

  // get field or fields
    for ( waitOp = false; ptrtop != ptrend && *ptrtop != ','; ++ptrtop, waitOp = !waitOp )
    {
      if ( waitOp )
      {
        if ( (ptrtop->uFlags & Token::is_punct) == 0 || *ptrtop != '+' )
          throw ParseError( "'+' expected" );
      }
        else
      {
        if ( (ptrtop->uFlags & Token::is_punct) != 0 )
          throw ParseError( "field expected" );
        fields.emplace_back( codepages::widetombcs( codepages::codepage_utf8, ptrtop->GetWideStr() ) );
      }
    }
    if ( !waitOp )
      throw ParseError( "unexpected end of field list" );

  // check for query
    if ( ++ptrtop >= ptrend )
      throw ParseError( "query expected" );

  // make unique fields
    std::sort( fields.begin(), fields.end() );
      fields.erase( std::unique( fields.begin(), fields.end() ), fields.end() );

  // create the subquery
    if ( fields.size() == 1 )
    {
      return mtc::zmap{
        { fn == Function::cover ? "cover" : "match", mtc::zmap{
          { "field", std::move( fields.front() ) },
          { "query", ParseQuery( ptrtop, ptrend ) } } } };
    }
      else
    {
      return mtc::zmap{
        { fn == Function::cover ? "cover" : "match", mtc::zmap{
          { "field", std::move( fields ) },
          { "query", ParseQuery( ptrtop, ptrend ) } } } };
    }
  }

  auto  ParseQuery( const widechar* str, size_t len ) -> mtc::zval
  {
    auto  intext = textAPI::Document{ { str, len } };
    auto  wbreak = context::Processor().WordBreak( intext );

    return ParseQuery( wbreak.GetTokens().data(), wbreak.GetTokens().data() + wbreak.GetTokens().size() );
  }

}}
