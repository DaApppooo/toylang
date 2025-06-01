#include "tl.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PLS_MAX 256

bool isoperator(int c)
{
  return (
     c == '!'
  || (c >= '#' && c <= '&')
  || c == '*'
  || c == '+'
  || c == '-'
  || c == '.'
  || c == '/'
  || c == ':'
  || (c >= '<' && c <= '@')
  || c == '^'
  || c == '~'
  );
}
bool isnum(int c)
{
  return c >= '0' && c <= '9';
}
uint64_t small_name_hash(const char* s, int len)
{
  if (len > 8) return 0;
  uint64_t r = 0;
  for (int i = 0; i < len; i++)
    r = (r << 8) | s[i];
  return r;
}
int find_string_global(TLState* TL, const char* s)
{
  for (int i = 0; i < TL->consts_len; i++)
  {
    if (tl_type_of_pro(TL->consts[i]) == TL_STR)
    {
      if (_tl_str_eq(tl_to_str_pro(TL->consts[i]), s))
      {
        return i;
      }
    }
  }
  return -1;
}



int int_const_from(TLState* TL, int64_t v)
{
  _TLInt* i = TL_MALLOC(sizeof(_TLInt));
  i->type = TL_INT;
  i->value = v;
  return _tl_consts_push(TL, i);
}

int float_const_from(TLState* TL, const char* s)
{
  _TLFloat* i = TL_MALLOC(sizeof(_TLFloat));
  i->type = TL_FLOAT;
  i->value = atof(s);
  return _tl_consts_push(TL, i);
}

int tl_find_const(TLState* TL, tl_object_t* p)
{
  for (int i = 0; i < TL->consts_len; i++)
  {
    if (TL->consts[i] == p)
      return i;
  }
  return -1;
}

static inline bool tok_is_brace(Token tok)
{ return tok.type == TOK_PARENTH || tok.type == TOK_BRACKET; }
bool is_call_or_index(Token* T, int i)
{
  return (
    i > 0
    && tok_is_brace(T[i])
    && (
      (
         T[i-1].type != TOK_OP
      && T[i-1].type != TOK_PARENTH
      && T[i-1].type != TOK_BRACKET
      )
    || T[i-1].rpned > 0
    )
  );
}

bool _tl_token_push(
  TLState* TL, TokenizerState* TS,
  TokenizerOut* out, char* btok,
  int* btoki, TokenType* current,
  int* depth, const char c,
  TokenType ctt
) {
  assert(out->toksi < TOKEN_STACK_MAX);
  btok[*btoki] = 0;
  bool stop_tokenizing = false;
  // printf("PUSH [%s] (type=%i)\n", btok, *current);
  switch (*current)
  {
  case TOK_FN:
  case TOK_DO:
  case TOK_ELIF:
  case TOK_IF:
  case TOK_WHILE:
  case TOK_END:
  case TOK_THEN:
  case TOK_BREAK:
  case TOK_CONTINUE:
  case TOK_RETURN:
  case TOK_ELSE:
  case TOK_CALL:
  case TOK_INDEX:
  case TOK_LC:
  case TOK_SMALL_INT:
  case TOK_NIL:
  case TOK_TRUE:
  case TOK_FALSE:
    assert(false);
    break;
  case TOK_NOTHING:
    break;
  case TOK_COMMA:
    out->toks[out->toksi++] = (Token){TOK_COMMA, 0, 0, 0};
    break;
  case TOK_INT:
    {
      int64_t i = strtoll(btok, NULL, 10);
      if (i <= 0x00ffffff)
        out->toks[out->toksi++] = (Token){TOK_SMALL_INT, i, 0};
      else
        out->toks[out->toksi++] = (Token){TOK_INT, int_const_from(TL, i), 0};
      break;
    }
  case TOK_FLOAT:
    out->toks[out->toksi++] = (Token){
      *current,
      float_const_from(TL, btok),
      0
    };
    break;
  case TOK_NAME:
    {
      const uint64_t name_hash = small_name_hash(btok, *btoki);
      switch (name_hash)
      {
        case HASH_FN: *current = TOK_FN; break;
        case HASH_RETURN: *current = TOK_RETURN; break;
        case HASH_TRUE: *current = TOK_TRUE; break;
        case HASH_FALSE: *current = TOK_FALSE; break;
        case HASH_NIL: *current = TOK_NIL; break;
        case HASH_IF: *current = TOK_IF; stop_tokenizing = true; break; 
        case HASH_ELIF: *current = TOK_ELIF; stop_tokenizing = true; break; 
        case HASH_ELSE: *current = TOK_ELSE; stop_tokenizing = true; break; 
        case HASH_WHILE: *current = TOK_WHILE; stop_tokenizing = true; break; 
        case HASH_DO: *current = TOK_DO; stop_tokenizing = true; break; 
        case HASH_THEN: *current = TOK_THEN; stop_tokenizing = true; break; 
        case HASH_BREAK: *current = TOK_BREAK; stop_tokenizing = true; break; 
        case HASH_CONTINUE: *current = TOK_CONTINUE; stop_tokenizing = true; break; 
        case HASH_END: *current = TOK_END; stop_tokenizing = true; break; 
      }
      if (*current != TOK_NAME)
      {
        out->toks[out->toksi++] = (Token){*current,0,0};
      }
      else
      {
        const TLName* const name = tl_get_name(TL->scope, btok);
        const int i = name->global_str_idx;
        if (
          tl_type_of_pro(name->data) == TL_FUNC
        ||tl_type_of_pro(name->data) == TL_CFUNC
        ) {
          if (tl_precedence_pro(name->data) != 255)
            *current = TOK_OP;
        }
        out->toks[out->toksi++] = (Token){*current, i, 0};
      }
      break;
    }
  case TOK_OP:
    {
      int i = find_string_global(TL, btok);
      if (i == -1)
      {
        i = _tl_consts_push_str(TL, btok);
      }
      out->toks[out->toksi++] = (Token){TOK_OP, i, 0};
      const TLName* name = tl_get_name_ex(TL->scope, i);
      if (name)
      {
        const TLType type = tl_type_of_pro(name->data);
        if (type == TL_FUNC || type == TL_CFUNC)
        {
          const uint8_t prec = tl_precedence_pro(name->data);
          if (prec > out->max_prec)
            out->max_prec = prec;
        }
      }
      break;
    }
  case TOK_PARENTH:
  case TOK_BRACKET:
    {
      if (btok[0] == ')' || btok[0] == ']')
      {
        int j;
        for (j = 0; j < out->toksi; j++)
        {
          if (
             out->toks[out->toksi-j-1].type == *current
          && out->toks[out->toksi-j-1].arg == UNSET_PARENTH
          ) {
            out->toks[out->toksi-j-1].arg = j;
            break;
          }
        }
        (*depth)--;
        tl_expect_r(j < out->toksi, true,
          "(Syntax) You forgor to open a parenthesis "
          "(one closed but none open before that).");
      }
      else
      {
        (*depth)++;
        out->toks[out->toksi++] = (Token){*current, UNSET_PARENTH, 0};
        if (is_call_or_index(out->toks, out->toksi-1) && out->max_prec < 10)
          out->max_prec = 10;
      }
      break;
    }
  case TOK_STR:
    {
      int offset = 1;
      const int len = *btoki-1;
      for (int i = 0; i < len-offset; i++)
      {
        btok[i] = btok[i+offset];
        if (btok[i] == '\\')
        {
          switch (btok[i+offset+1])
          {
            case 'n': btok[i] = '\n'; offset++; break;
            case 'r': btok[i] = '\r'; offset++; break;
            case 'e': btok[i] = '\e'; offset++; break;
            case 'b': btok[i] = '\b'; offset++; break;
            case 'a': btok[i] = '\a'; offset++; break;
            case '"': btok[i] = '"'; offset++; break;
            case '\'': btok[i] = '\''; offset++; break;
            case '\\': btok[i] = '\\'; offset++; break;
            case 'x':
              {
                assert(len > i+offset+3);
                const char digits[3] = {
                  btok[i+offset+2], btok[i+offset+3], 0
                };
                btok[i] = strtol(digits, NULL, 16);
                offset += 3;
                break;
              }
          }
        }
      }
      btok[len-offset] = 0;
      out->toks[out->toksi++] = (Token){
        *current,
        _tl_consts_push_str(TL, btok),
        0
      };
      break;
    }
  }
  // if (*current != TOK_NOTHING)
  *btoki = 0;
  *current = ctt;
  return stop_tokenizing;
}
bool is_single_token(TokenType t)
{
  return (
     t == TOK_PARENTH
  || t == TOK_COMMA
  || t == TOK_BRACKET
  );
}
TokenizerOut tl_tokenize_string(TLState *G, TokenizerState* TS, char *code)
{
  TokenizerOut out;
  char btok[TOKEN_STR_MAX];
  int btoki = 0;
  int depth = 0;
  TokenType current = TOK_NOTHING;
  TokenType ctt;
  char c;
  out.max_prec = out.toksi = 0;
  if (TS->len == 0)
    return TOKENIZER_EOF;
  for (; TS->i < TS->len; TS->i++)
  {
    c = code[TS->i];
    if (c == '\n' || c == ';')
    {
      if (depth == 0 || (depth == 1 && (btok[0] == ')' || btok[0] == ']')))
        goto FINISH;
      else continue;
    }
    if (btoki > 0 && (btok[0] == '"' || btok[0] == '\''))
    {
      if (
        (c == '"' || c == '\'') &&
        (
          btok[btoki-1] != '\\' ||
          (btoki > 1 && btok[btoki-2] == '\\')
        )
      )
      { btok[btoki++] = c; c = 0; ctt = TOK_NOTHING; }
      else
        ctt = TOK_STR;
    }
    else if (c == '"' || c == '\'')
      ctt = TOK_STR;
    else if (c == ',')
      ctt = TOK_COMMA;
    else if (c == ':')
    {
      if (current == TOK_NAME)
        ctt = TOK_NAME;
      else if (current == TOK_INT)
      { current = TOK_FLOAT; ctt = TOK_FLOAT; }
      else
        current = TOK_FLOAT;
    }
    else if (isalpha(c) || c == '_')
    {
      if (btoki == 1 && btok[0] == ':')
        current = TOK_NAME;
      ctt = TOK_NAME;
    }
    else if (isoperator(c))
    {
      if (current == TOK_INT && c == '.')
      {
        current = TOK_FLOAT;
        ctt = TOK_FLOAT;
      }
      else
        ctt = TOK_OP;
    }
    else if (isnum(c))
    {
      if (current == TOK_NAME)
        ctt = TOK_NAME;
      else if (current == TOK_FLOAT)
        ctt = TOK_FLOAT;
      else if (btoki > 1 && btok[btoki-1] == '.' && current == TOK_OP)
      {
        current = TOK_FLOAT;
        ctt = TOK_FLOAT;
      }
      else
        ctt = TOK_INT;
    }
    else if (c == '(' || c == ')')
    {
      ctt = TOK_PARENTH;
      if (out.toksi > 2 && out.toks[0].type == TOK_FN && c == ')')
        goto FINISH;
    }
    else if (c == '[' || c == ']')
    {
      ctt = TOK_BRACKET;
    }
    else if (c == ' ' || c == '\t')
      ctt = TOK_NOTHING;
    
    if (ctt != current || is_single_token(ctt))
    {
      if (_tl_token_push(
        G, TS, &out, btok, &btoki,
        &current, &depth, c, ctt
      ))
      {
        goto FINISH;
      }
    }
    if (ctt != TOK_NOTHING)
      btok[btoki++] = c;
  }
FINISH:
  _tl_token_push(
    G, TS, &out, btok, &btoki,
    &current, &depth, 0, current
  );
  // printf("Test %i\n", (int)c);
  if (c == ')' && c != 0)
  {
    btok[0] = ')';
    btoki = 1;
    current = TOK_PARENTH;
    _tl_token_push(
      G, TS, &out, btok, &btoki,
      &current, &depth, 0, current
    );
  }
  TS->i++;
  return out;
}

// void _bc_pre_add(TLState* TL, int count)
// {
//   if (TL->bclen+count <= TL->bccap)
//     return;
//   while (TL->bccap < TL->bclen+count)
//     TL->bccap += TL->bccap/2 + (TL->bccap < 2);
//   if (TL->bytecode)
//     TL->bytecode = realloc(TL->bytecode, TL->bccap*sizeof(uint32_t));
//   else
//     TL->bytecode = calloc(TL->bccap, sizeof(uint32_t));
// }
void _bc_push(TLState* G, uint32_t code)
{
  G->bc_len++;
  if (G->bc_len > G->bc_cap)
  {
    G->bc_cap += G->bc_cap/2 + (G->bc_cap < 2);
    if (G->bc)
      G->bc = realloc(G->bc, G->bc_cap*sizeof(uint32_t));
    else
      G->bc = calloc(G->bc_cap, sizeof(uint32_t));
  }
  G->bc[G->bc_len-1] = code;
}


typedef enum BraceType
{
  PARENTHESIS,
  BRACKET
} BraceType;
typedef struct BraceInfo
{
  BraceType type;
  int pos;
} BraceInfo;
typedef struct BraceManager
{
  BraceInfo infos[PLS_MAX];
  int len;
} BraceManager;
BraceManager init_brace_management()
{
  BraceManager ret;
  ret.len = 0;
  return ret;
}
void update_brace_manager(BraceManager* self, Token* T,
                          Token current_token, int i)
{
  while (self->len > 0 && self->infos[self->len-1].pos + T[self->infos[self->len-1].pos].arg < i)
    self->len--;
}
void update_debug_brace_manager(BraceManager* self, Token* T,
                                Token current_token, int i)
{
  while (self->len > 0 && self->infos[self->len-1].pos + T[self->infos[self->len-1].pos].arg < i)
  {
    self->len--;
    if (self->infos[self->len].type == PARENTHESIS)
      printf(" ) ");
    else if (self->infos[self->len].type == BRACKET)
      printf(" ] ");
    else
      assert(false);
  }
}
int current_depth(BraceManager* self)
{ return self->len; }
void push_brace(BraceManager* self, Token tok, int pos)
{
  BraceType type;
  if (tok.type == TOK_PARENTH)
    type = PARENTHESIS;
  else if (tok.type == TOK_BRACKET)
    type = BRACKET;
  else
    assert(false);
  self->infos[self->len++] = (BraceInfo){.type=type, .pos=pos};
}

void tl_braces_to_rpn(
  TLState* G,
  TokenizerOut* toks,
  int start, int len
) {
  Token* T = toks->toks+start;
  for (int i = 0; i < len; i++)
  {
    if (tok_is_brace(T[i]) && !is_call_or_index(T, i))
    {
      if (T[i].type == TOK_PARENTH)
      {
        tl_expr_to_rpn(G, toks, i+1+start, T[i].arg);
        if (T[i+1].rpned)
        {
          T[i].rpned = T[i+1].rpned+1;
          T[i+T[i].rpned-1].rpned = T[i].rpned;
        }
        i += T[i].arg;
      }
      else if (T[i].type == TOK_BRACKET)
      {
        if (T[i].arg == 0)
        {
          T[i].rpned = 1;
          continue;
        }
        BraceManager bm = init_brace_management();
        const int list_len = T[i].arg;
        int elem_start = 0;
        for (int j = 0; j < list_len; j++)
        {
          update_brace_manager(
            &bm, T, T[i+j+1],
            i+j+1
          );
          if (tok_is_brace(T[i+j+1]))
            push_brace(&bm, T[i+j+1], i+j+1);
          else if (
             T[i+j+1].type == TOK_COMMA
          && current_depth(&bm) == 0
          ) {
            if (T[i].arg == list_len)
              T[i].arg = j - elem_start;
            else
            {
              T[i+elem_start].type = TOK_LC;
              T[i+elem_start].arg = j - elem_start;
            }
            tl_expr_to_rpn(
               G, toks,
               i+elem_start+1 + start,
               j-elem_start
            );
            elem_start = j+1;
          }
        }
        if (T[i].arg != list_len)
        {
          T[i+elem_start].type = TOK_LC;
          T[i+elem_start].arg = list_len - elem_start;
        }
        tl_expr_to_rpn(
           G, toks,
           i+elem_start+1 + start,
           list_len-elem_start
        );
        T[i].rpned = list_len+1;
        T[i].op_argc = list_len;
        T[i+T[i].rpned-1].rpned = list_len+1;
        i += list_len;
      }
      else
        assert(false);
    }
  }
}

void tl_ops_to_rpn(
  TLState* G,
  TokenizerOut* toks,
  int start, int len
) {
  Token* const T = toks->toks+start;
  BraceManager bm;
  for (int prec = toks->max_prec; prec >= 0; prec--)
  {
    bm = init_brace_management();
    for (int i = 0; i < len; i++)
    {
      if (T[i].rpned != 0)
      {
        i += T[i].rpned - 1;
        continue;
      }
      // printf("BEFORE: (prec=%i/%i)\n",
      //        prec, toks->max_prec);
      // tl_debug_token_array(G, T, len, i);
      update_brace_manager(&bm, T, T[i], i);
      if (is_call_or_index(T, i) && prec == 10)
      {
        TokenType call_type;
        if (T[i].type == TOK_PARENTH)
          call_type = TOK_CALL;
        else if (T[i].type == TOK_BRACKET)
          call_type = TOK_INDEX;
        else
          assert(false);
        // Call
        // f ( a , b ) -> a b f ()
        // {(f + g)(a, b) ->} f g + ( a , b ) -> a b f g + ()
        if (T[i-1].rpned == 0)
          T[i-1].rpned = 1;
      
        // currently: f g + ( a b + , c
        if (T[i].arg == 0)
        {
          // f g + (
          // ^-+-+-+
          // +-+-+-^
          T[i-T[i-1].rpned].rpned = T[i-1].rpned+1;
          T[i].rpned = T[i-1].rpned+1;
          T[i].op_argc = 1; // 1 for the function being called
          T[i].type = call_type;
          continue;
        }
        int argc = 1;
        int param_start = 0;
        {
          for (int param_tok = 0; param_tok < T[i].arg; param_tok++)
          {
            update_brace_manager(
              &bm, T, T[i+param_tok+1],
              i+param_tok+1
            );
            if (tok_is_brace(T[i+param_tok+1]))
              push_brace(&bm, T[i+param_tok+1], i+param_tok+1);
            else if (
               T[i+param_tok+1].type == TOK_COMMA
            && current_depth(&bm) == 0
            ) {
              tl_expr_to_rpn(
                 G, toks,
                 i+param_start+1 + start,
                 param_tok-param_start
              );
              param_start = param_tok;
              argc++;
            }
          }
          tl_expr_to_rpn(
             G, toks,
             i+param_start+1 + start,
             T[i].arg-param_start
          );
        }
        // currently: f g + ( a b + , c
        // move over the parenthesis
        Token parenth = T[i];
        parenth.type = call_type;
        memmove(T+i, T+i+1, parenth.arg*sizeof(Token));
        T[i+parenth.arg] = parenth;
        // currently: f g + a b + , c (
        //                  [-------]    << temp.arg
        //                  ^            << i
        //            [---]              << T[i-1].rpned
        // goal:      a b + , c f g + (
        const int to_move = T[i-1].rpned;
        for (int imove = 0; imove < to_move; imove++)
        {
          //          f g + a b + , c (
          // imove=0: f g a b + , c + (
          // imove=1: f a b + , c g + (
          // imove=2: a b + , c f g + (
          Token temp = T[i-imove-1];
          memmove(T+i-imove-1, T+i-imove, parenth.arg*sizeof(Token));
          T[i+parenth.arg-imove-1] = temp;
        }
        // Finally, set .rpned and arg count
        T[i+parenth.arg].rpned = to_move + parenth.arg + 1;
        T[i+parenth.arg].op_argc = argc+1;
        T[i-to_move].rpned = to_move + parenth.arg + 1;
      }
      else
      {
        const TLName* name = tl_has_name_ex(G->scope, T[i].arg);
        tl_object_t* op = name ? name->data : NULL;
        if (T[i].type == TOK_OP && tl_type_of_pro(op) == TL_NIL)
        {
          tl_throw(
            "ERROR: (Syntax) Use of unknown operator '%s'.\n",
            tl_to_str_pro(G->consts[T[i].arg])
          );
          return;
        }
        if (
           T[i].type == TOK_OP
        && tl_precedence_pro(op) == prec
        ) {
          if (i == 0 || (T[i-1].rpned == 0 && T[i-1].type == TOK_OP))
          {
            tl_expect_r(
              len > 1,,
              "(Syntax) A single operator in a line doesn't mean anything."
            );
            // '-b' in 'a+-b'
            // '-(a+b)' in '-(a+b)+c'
            const int right_len = T[i+1].rpned ? T[i+1].rpned : 1;
            Token temp = T[i];
            memmove(T+i, T+i+1, sizeof(Token)*right_len);
            temp.op_argc = 1;
            T[i+right_len] = temp;
            T[i].rpned = 1+right_len;
            T[i+right_len].rpned = 1+right_len;
            i += right_len; //+1 (after temp) -1 (for loop increment)
          }
          else
          {
            tl_expect_r(
              len > 2,,
              "(Syntax) Expected EXPR OP EXPR expression, "
              "but your code said no T-T"
            );
            // 'a+b' in 'a+b+c' (depth = 0)
            // 'a+b' in '(a+b)*c' (depth = 1)
            const int left_len = T[i-1].rpned ? T[i-1].rpned : 1;
            const int right_len = T[i+1].rpned ? T[i+1].rpned : 1;
            Token temp = T[i];
            memmove(T+i, T+i+1, sizeof(Token)*right_len);
            temp.op_argc = 2;
            T[i+right_len] = temp;
            T[i-left_len].rpned = 1+left_len+right_len;
            T[i+right_len].rpned = 1+left_len+right_len;
            i += right_len; //+1 (after temp) -1 (for loop increment)
          }
        }
      }
      // printf("AFTER: (prec=%i/%i)\n",
      //        prec, toks->max_prec);
      // tl_debug_token_array(G, T, len, i);
    }
  }
}

void tl_expr_to_rpn(
  TLState* G,
  TokenizerOut* toks,
  int start, int len)
{
  // printf("[%i to %i]\n", start, start+len);
  tl_braces_to_rpn(G, toks, start, len);
  if (tl_errored()) return;
  tl_ops_to_rpn(G, toks, start, len);
  if (tl_errored()) return;
}

void tl_debug_token_array(
  TLState* TL,
  Token *a, int len, int current)
{
  BraceManager bm = init_brace_management();
  for (int i = 0; i < len; i++)
  {
    update_debug_brace_manager(&bm, a, a[i], i);
    if (i == current)
    { printf(" {"); }
    if (a[i].rpned)
    { printf(" <%i>*", a[i].rpned); }
    else
    { printf(" "); }
    switch (a[i].type)
    {
      case TOK_NAME:
      case TOK_OP:
        printf("%s ", tl_to_str_pro(TL->consts[a[i].arg]));
        break;
      case TOK_BREAK: printf("break "); break;
      case TOK_CONTINUE: printf("continue "); break;
      case TOK_COMMA: printf(", "); break;
      case TOK_IF: printf("if "); break;
      case TOK_ELIF: printf("elif "); break;
      case TOK_FLOAT: printf("%lf ", tl_to_float_pro(TL->consts[a[i].arg])); break;
      case TOK_INT: printf("%li ", tl_to_int_pro(TL->consts[a[i].arg])); break;
      case TOK_SMALL_INT: printf("%u ", a[i].arg); break;
      case TOK_NOTHING: printf("(/) "); break;
      case TOK_FN: printf("fn "); break;
      case TOK_CALL: printf("<call> "); break;
      case TOK_DO: printf("do "); break;
      case TOK_ELSE: printf("else "); break;
      case TOK_END: printf("end "); break;
      case TOK_THEN: printf("then "); break;
      case TOK_WHILE: printf("while "); break;
      case TOK_RETURN: printf("return "); break;
      case TOK_NIL: printf("nil "); break;
      case TOK_TRUE: printf("true "); break;
      case TOK_FALSE: printf("false "); break;
      case TOK_STR: printf("(str)[%s]", tl_to_str_pro(TL->consts[a[i].arg])); break;
      case TOK_PARENTH:
        printf("[%i]( ", a[i].arg);
        push_brace(&bm, a[i], i);
        break;
      case TOK_BRACKET:
        printf("[%i][ ", a[i].arg);
        push_brace(&bm, a[i], i);
        break;
      case TOK_INDEX: printf("<index> "); break;
      case TOK_LC: printf("<lc> "); break;
    }
    if (i == current)
    { printf("} "); }
  }
  puts("");
}
void tl_rpn_to_bytecode(TLState* G, TokenizerOut* rpn, int start, int len)
{
  int bra[64];
  int bls = 0;
  for (int i = start; i < start+len; i++)
  {
    while (bls > 0 && bra[bls-1] + rpn->toks[bra[bls-1]].op_argc < i)
    {
      bls--;
      _bc_push(G, CODE(TLOP_LE, 0));
    }
    if (rpn->toks[i].type == TOK_NAME || rpn->toks[i].type == TOK_OP)
    {
      _bc_push(G, CODE(TLOP_COPY_NAME, rpn->toks[i].arg));
      if (rpn->toks[i].type == TOK_OP)
        _bc_push(G, CODE(TLOP_CALL, rpn->toks[i].op_argc));
    }
    else if (rpn->toks[i].type == TOK_CALL)
    {
      TLName* n = tl_get_name(G->scope, "()");
      _bc_push(G, CODE(TLOP_COPY_NAME, n->global_str_idx));
      _bc_push(G, CODE(TLOP_CALL, rpn->toks[i].op_argc));
    }
    else if (rpn->toks[i].type == TOK_INDEX)
    {
      TLName* n = tl_get_name(G->scope, "[]");
      _bc_push(G, CODE(TLOP_COPY_NAME, n->global_str_idx));
      _bc_push(G, CODE(TLOP_CALL, rpn->toks[i].op_argc));
    }
    else if (rpn->toks[i].type == TOK_BRACKET)
    {
      bra[bls++] = i;
      _bc_push(G, CODE(TLOP_LB, 0));
    }
    else if (rpn->toks[i].type == TOK_LC && bls > 0)
    {
      _bc_push(G, CODE(TLOP_LC, 0));
    }
    else if (rpn->toks[i].type == TOK_COMMA
          || rpn->toks[i].type == TOK_PARENTH)
      ;
    else if (rpn->toks[i].type == TOK_SMALL_INT)
      _bc_push(G, CODE(TLOP_COPY_SMALL_INT, rpn->toks[i].arg));
    else if (rpn->toks[i].type == TOK_TRUE)
      _bc_push(G, CODE(TLOP_COPY_BOOL, 1));
    else if (rpn->toks[i].type == TOK_FALSE)
      _bc_push(G, CODE(TLOP_COPY_BOOL, 0));
    else if (rpn->toks[i].type == TOK_NIL)
      _bc_push(G, CODE(TLOP_COPY_NIL, 0));
    else
    {
      const TokenType T = rpn->toks[i].type;
      tl_expect_r(
        T == TOK_INT ||
        T == TOK_STR ||
        T == TOK_FLOAT,,
        "(Syntax) Expected constant, got something else."
      );
      _bc_push(G, CODE(TLOP_COPY_CONST, rpn->toks[i].arg));
    }
  }
  while (bls > 0)
  {
    bls--;
    _bc_push(G, CODE(TLOP_LE, 0));
  }
}

void tl_parse_to_bytecode(TLState* TL, char* code)
{
  TokenizerState TS;
  int cond_depth = 0;
  TS.i = 0;
  TS.len = _tl_str_len(code);
  while (TS.i < TS.len)
  {
    const int _prev_i = TS.i;
    TokenizerOut toks = tl_tokenize_string(TL, &TS, code);
    if (tl_errored()) return;
    if (toks.toksi == 0)
    {
      assert(TS.i != _prev_i);
      continue;
    }
    switch (toks.toks[toks.toksi-1].type)
    {
    case TOK_END:
    case TOK_ELSE:
    case TOK_IF:
    case TOK_ELIF:
    case TOK_THEN:
    case TOK_DO:
    case TOK_BREAK:
    case TOK_WHILE:
    case TOK_CONTINUE:
      toks.toksi--;
      break;
    default:
      break;
    }
    if (toks.toks[0].type == TOK_FN)
    {
      tl_expect_r(toks.toksi >= 3,,
        "(Syntax) Function declaration expects at least a name and empty "
        "parenthesis."
      );
      uint32_t prec = 255;
      int argc = 0;
      int offset = 0;
      if (toks.toks[1].type == TOK_INT)
      {
        // 'fn' <prec> <func_name> '(' ...
        prec = tl_to_int_pro(TL->consts[toks.toks[1].arg]);
        offset = 1;
      }
      // else
      //  'fn' <func_name> '(' ...

      _bc_push(TL, CODE(TLOP_FNH, TL->consts_len));
      int name_len = 0;
      while (toks.toks[1+offset+name_len].type != TOK_PARENTH
            && 1+offset+name_len < toks.toksi)
      { name_len++; }
      tl_expect_r(1+offset+name_len < toks.toksi,,
                  "(Syntax) Missing parenthesis in function declaration.");
      tl_expr_to_rpn(TL, &toks, 1+offset, name_len);
      tl_rpn_to_bytecode(TL, &toks, 1+offset, name_len);
      offset += name_len;
      _bc_push(TL, CODE(TLOP_FN, 0));
      
      const int bc_pos_on_call = TL->bc_len;
      for (int i = 2+offset; i < toks.toksi; i++)
      {
        if (toks.toks[i].type == TOK_COMMA)
          continue;
        assert(toks.toks[i].type == TOK_NAME);
        if (_tl_str_eq(
            tl_to_str_pro(TL->consts[toks.toks[i].arg]),
            "self"
        ))
          _bc_push(TL, CODE(TLOP_SELF, argc));
        else
        {
          _bc_push(TL, CODE(TLOP_NEW_NAME, toks.toks[i].arg));
          _bc_push(TL, CODE(TLOP_PARAM, argc));
        }
        argc++;
      }

      if (toks.toks[2].type == TOK_OP)
      {
        tl_expect_r(argc == 1 || argc == 2,,
            "(Syntax) Operators can only have 1 or 2 parameters.");
      }

      // obj = TL->scope->names[toks.toks[1+offset].arg].data;
      // if (tl_type_of_pro(obj) != TL_NIL)
      // {
      //   int str_i = find_string_global(TL, "super");
      //   if (str_i == -1)
      //     str_i = _tl_consts_push_str(TL, "super");
      //   const int func_idx = tl_find_const(TL, obj);
      //   _bc_push(TL, CODE(TLOP_COPY_CONST, func_idx));
      //   _bc_push(TL, CODE(TLOP_ASSIGN_NAME, str_i));
      //   _bc_push(TL, CODTLOP_NEW_NAMEE(TLOP_POP, 1));
      // }
      _TLFunc* f = TL_MALLOC(sizeof(_TLFunc));
      f->argc = argc;
      f->bytecode_pt = bc_pos_on_call;
      f->prec = prec;
      f->type = TL_FUNC;
      _tl_consts_push(TL, f);
      cond_depth++;
    }
    else if (toks.toks[0].type == TOK_RETURN)
    {
      int i = 1;
      int ret_count = 0;
      while (i < toks.toksi)
      {
        const int start = i;
        while (i < toks.toksi && toks.toks[i].type != TOK_COMMA)
          i++;
        ret_count++;
        tl_expr_to_rpn(TL, &toks, start, i-start);
        if (tl_errored()) return;
        tl_rpn_to_bytecode(TL, &toks, start, i-start);
        if (tl_errored()) return;
        i++;
      }
      _bc_push(TL, CODE(TLOP_RET, ret_count));
    }
    else
    {
      tl_expr_to_rpn(TL, &toks, 0, toks.toksi);
      if (tl_errored()) return;
      tl_rpn_to_bytecode(TL, &toks, 0, toks.toksi);
      if (tl_errored()) return;
    }
    switch (toks.toks[toks.toksi].type)
    {
    case TOK_BREAK:
      _bc_push(TL, CODE(TLOP_BREAK, cond_depth));
      continue;
    case TOK_CONTINUE:
      _bc_push(TL, CODE(TLOP_CONTINUE, cond_depth));
      continue;
    case TOK_IF:
      cond_depth++;
      _bc_push(TL, CODE(TLOP_COND, cond_depth));
      continue;
    case TOK_ELIF:
      _bc_push(TL, CODE(TLOP_COND, cond_depth));
      continue;
    case TOK_WHILE:
      cond_depth++;
      _bc_push(TL, CODE(TLOP_COND, cond_depth));
      continue;
    case TOK_THEN:
      _bc_push(TL, CODE(TLOP_IF, cond_depth));
      continue;
    case TOK_DO:
      _bc_push(TL, CODE(TLOP_WHILE, cond_depth));
      continue;
    case TOK_ELSE:
      _bc_push(TL, CODE(TLOP_ELSE, cond_depth));
      continue;
    case TOK_END:
      // if (cond_depth == func_depths_stack[func_depths_sp-1])
      // {
      //   _bc_push(TL, CODE(TLOP_RET, 0));
      // }
      _bc_push(TL, CODE(TLOP_END, cond_depth));
      cond_depth--;
      continue;
    default:
      break;
    }
    toks.toks[toks.toksi].type = TOK_NOTHING;
    _bc_push(TL, CODE(TLOP_EOS, 0));
  }
}

void tl_compile_string(TLState *TL, char *ref_code)
{}

