
// tlbc.c
#include "tl.h"
#include <assert.h>

int tl_run_bytecode_ex(TLScope *TL, int len, int depth,
                       int param_count) {
  char _buf[16];
  uint64_t conditional_block_info = 0;
  int fnh_register = -1;
  const int scope_depth = depth;
#define IS_LOOP() (conditional_block_info & 1)
#define PUSH_LOOP() conditional_block_info = (conditional_block_info << 1) | 1
#define PUSH_NOLOOP() conditional_block_info = (conditional_block_info << 1) | 0
#define POP_CBI() conditional_block_info >>= 1
  const int bc_len = len;
  uint32_t* const bc = TL->global->bc;
  for (uint32_t code = TL->bc_pos < bc_len ? bc[TL->bc_pos++] : TLOP_EXIT;
       tlbc_opcode(code) != TLOP_EXIT;
       code = TL->bc_pos < bc_len ? bc[TL->bc_pos++] : TLOP_EXIT) {
    switch (tlbc_opcode(code))
    {
    case TLOP_EXIT:
      return 0;
      break;
    case TLOP_FNH:
      assert(tlbc_arg(code) >= 0);
      assert(tlbc_arg(code) < TL->global->consts_len);
      fnh_register = tlbc_arg(code);
      break;
    case TLOP_FN:
      {
      assert(tlbc_arg(code) >= 0);
      assert(tlbc_arg(code) < TL->global->consts_len);
      assert(fnh_register != -1);
      // const int pos = tl_push(TL, TL->global->consts[fnh_register]);
      assert(tl_top_ex(TL)->ref);
      tl_top_ex(TL)->ref->data = TL->global->consts[fnh_register];
      _tl_hold(TL->global, TL->global->consts[fnh_register]);
      fnh_register = -1;
      do
        code = bc[TL->bc_pos++];
      while (code != CODE(TLOP_END, depth + 1));
      break;
    }
    case TLOP_COPY_NAME:
      {
      assert(tlbc_arg(code) >= 0);
      assert(tlbc_arg(code) < TL->global->consts_len);
      TLName* name =
          tl_get_name(TL, tl_to_str_pro(TL->global->consts[tlbc_arg(code)]));
      tl_push(TL, name->data);
      tl_top_ex(TL)->ref = name;
      break;
    }
    case TLOP_NEW_NAME:
      assert(tlbc_arg(code) >= 0);
      assert(tlbc_arg(code) < TL->global->consts_len);
      tl_set_name_ex(TL, tlbc_arg(code), NULL);
      break;
    case TLOP_ASSIGN_NAME:
      {
      assert(tlbc_arg(code) >= 0);
      assert(tlbc_arg(code) < TL->global->consts_len);
      const int gsi = tlbc_arg(code);
      TLScope *pt = TL;
      int up = 0;
      bool brk = false;
      do
      {
        for (int i = 0; i < pt->name_count; i++)
        {
          if (pt->names[i].global_str_idx == gsi)
          {
            _tl_drop(TL->global, pt->names[i].data);
            pt->names[i].data = tl_top(TL);
            _tl_hold(TL->global, pt->names[i].data);
            tl_top_ex(TL)->ref = pt->names + i;
            brk = true;
            break;
          }
        }
        if (brk || pt->parent == NULL)
          break;
        pt = pt->parent;
        up++;
      } while (true);
      if (!brk)
      {
        const int i = tl_set_name_ex(TL, gsi, NULL);
        tl_top_ex(TL)->ref = TL->names + i;
      }
      break;
      }
    case TLOP_POP:
      assert(tlbc_arg(code) <= TL->stack_top);
      for (int i = 0; i < tlbc_arg(code); i++)
        tl_pop(TL);
      break;
    case TLOP_EOS:
      while (TL->stack_top > 0)
        tl_pop(TL);
      break;
    case TLOP_COPY_CONST:
      assert(TL->stack_top < TL->stack_cap);
      tl_push(TL, TL->global->consts[tlbc_arg(code)]);
      tl_top_ex(TL)->ref = NULL;
      break;
    case TLOP_CALL:
      {
        tl_call(TL, tlbc_arg(code));
        break;
      }
    case TLOP_PARAM:
      assert(tlbc_arg(code) >= 0);
      assert(tlbc_arg(code) < TL->name_count);
      assert(depth >= 1);
      if (param_count <= 0)
        TL->names[tlbc_arg(code)].data = NULL;
      else
      {
        TL->names[tlbc_arg(code)].data =
            TL->parent->stack[TL->parent->stack_top - param_count - 1].object;
        TL->parent->stack[TL->parent->stack_top - param_count - 1].ref =
            TL->names + tlbc_arg(code);
        param_count--;
      }
      break;
    case TLOP_COND: {
      if (tlbc_arg(code) == depth) {
        do
          code = bc[TL->bc_pos++];
        while (tlbc_opcode(code) != TLOP_END || tlbc_arg(code) > depth);
        assert(tlbc_arg(code) == depth);
        assert(!IS_LOOP());
        POP_CBI();
        depth--;
      } else if (tlbc_arg(code) > depth) {
        assert(tlbc_arg(code) == depth + 1);
        depth++;
      } else {
        // crash the program coz this branch isn't supposed to be reached
        // ever.
        assert(tlbc_arg(code) >= depth /* END SHOULD HAVE OCCURED */);
      }
      break;
    }
    case TLOP_IF:
    {
      assert(tlbc_arg(code) == depth);
      if (tl_to_bool(TL))
        PUSH_NOLOOP();
      else {
        do
          code = bc[TL->bc_pos++];
        while (!(((tlbc_opcode(code) == TLOP_COND) ||
                  (tlbc_opcode(code) == TLOP_ELSE) ||
                  (tlbc_opcode(code) == TLOP_END)) &&
                 tlbc_arg(code) == depth));
        if (tlbc_arg(code) == TLOP_END)
          depth--;
      }
      tl_pop(TL); // pop boolean
      break;
    }
    case TLOP_ELSE:
      assert(tlbc_arg(code) == depth);
      if (IS_LOOP()) {
        do
          code = bc[--TL->bc_pos];
        while (!(tlbc_opcode(code) == TLOP_COND && tlbc_arg(code) == depth));
        TL->bc_pos++;
      } else {
        do
          code = bc[TL->bc_pos++];
        while (!(tlbc_opcode(code) == TLOP_END && tlbc_arg(code) == depth));
        depth--;
      }
      POP_CBI();
      break;
    case TLOP_WHILE: {
      assert(tlbc_arg(code) == depth);
      if (tl_to_bool(TL))
        PUSH_LOOP();
      else {
        do
          code = bc[TL->bc_pos++];
        while (!(((tlbc_opcode(code) == TLOP_ELSE) ||
                  (tlbc_opcode(code) == TLOP_END)) &&
                 tlbc_arg(code) == depth));
        if (tlbc_arg(code) == TLOP_END)
          depth--;
      }
      tl_pop(TL); // pop boolean
      break;
    }
    case TLOP_BREAK:
      do {
        if (tlbc_opcode(code) == TLOP_END && tlbc_arg(code) == depth) {
          depth--;
          POP_CBI();
        }
        code = bc[TL->bc_pos++];
      } while (!(tlbc_opcode(code) == TLOP_END && tlbc_arg(code) == depth &&
                 IS_LOOP()));
      depth--;
      break;
    case TLOP_CONTINUE:
      do {
        if (tlbc_opcode(code) == TLOP_COND && tlbc_arg(code) == depth) {
          depth--;
          POP_CBI();
        }
        code = bc[--TL->bc_pos];
      } while (!(tlbc_opcode(code) == TLOP_COND && tlbc_arg(code) == depth &&
                 IS_LOOP()));
      TL->bc_pos++;
      break;
    case TLOP_RET:
      return tlbc_arg(code);
    case TLOP_LB:
      {
        int list_depth = 0;
        int start = TL->bc_pos;
        int i = TL->bc_pos;
        int prev_stack_pos = 0;
        int elemc = 0;
        TLScope* child = tl_new_scope_ex(
          TL->global, TL, TL->stack_cap,
          TL->max_name_count, i
        );
        if (tlbc_arg(bc[i-1]) != 0)
          i += tlbc_arg(bc[i-1]);
        while (tlbc_opcode(bc[i]) != TLOP_LE || list_depth > 0)
        {
          if (list_depth > 0)
          {
            if (tlbc_opcode(bc[i]) == TLOP_LE)
              list_depth--;
            else if (tlbc_opcode(bc[i]) == TLOP_LB)
              list_depth++;
            i++;
          }
          else
          {
            if (tlbc_opcode(bc[i]) == TLOP_LB)
            {
              list_depth++;
              i += tlbc_arg(bc[i]);
            }
            else if (tlbc_opcode(bc[i]) == TLOP_LC)
            {
              bc[start-1] = CODE(tlbc_opcode(bc[start-1]), i - start);
              child->bc_pos = start;
              tl_run_bytecode_ex(child, i, depth, 0);
              int name_add = 0;
              if (child->stack_top > prev_stack_pos)
                name_add = child->stack_top - prev_stack_pos;
              else
              {
                tl_push_nil(child);
                name_add = 1;
              }
              for (int i = 0; i < name_add; i++)
              {
                _tl_idstr_from_int(_buf, elemc+i, sizeof(_buf));
                TLName* const name = tl_get_local(child, _buf);
                _tl_drop(TL->global, name->data);
                name->data = child->stack[prev_stack_pos+i].object;
                child->stack[prev_stack_pos+i].ref = name;
                _tl_hold(TL->global, name->data);
              }
              elemc += name_add;
              prev_stack_pos = child->stack_top;
              start = i+1;
              if (tlbc_arg(bc[i]))
                i += tlbc_arg(bc[i]) - 1;
            }
            i++;
          }
        }
        if (i != start)
        {
          bc[start-1] = CODE(tlbc_opcode(bc[start-1]), i - start);
          child->bc_pos = start;
          tl_run_bytecode_ex(child, i, depth, 0);
          start = i+1;
          int name_add;
          if (child->stack_top > prev_stack_pos)
            name_add = child->stack_top - prev_stack_pos;
          else
          {
            tl_push_nil(child);
            name_add = 1;
          }
          for (int i = 0; i < name_add; i++)
          {
            _tl_idstr_from_int(_buf, elemc+i, sizeof(_buf));
            TLName* const name = tl_get_local(child, _buf);
            _tl_drop(TL->global, name->data);
            name->data = child->stack[prev_stack_pos+i].object;
            child->stack[prev_stack_pos+i].ref = name;
            _tl_hold(TL->global, name->data);
          }
        }
        _TLScope* virt = TL_MALLOC(sizeof(_TLScope));
        virt->scope = child;
        virt->type = TL_SCOPE;
        TL->bc_pos = i+1;
        tl_push(TL, virt);
        break;
      }
    case TLOP_END:
      assert(depth > 0);
      if (depth == scope_depth)
        return 0;
      if (IS_LOOP()) {
        do
          code = bc[--TL->bc_pos];
        while (!(tlbc_opcode(code) == TLOP_COND && tlbc_arg(code) == depth));
        TL->bc_pos++;
      } else
        depth--;
      POP_CBI();
      break;
    case TLOP_LE:
    case TLOP_LC:
      assert(false);
    }
  }
  return 0;
}
void tl_run_bytecode(TLState *TL) { tl_run_bytecode_ex(TL->scope, TL->bc_len, 0, 0); }

// tlpr.c
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



int int_const_from(TLState* TL, const char* s)
{
  _TLInt* i = TL_MALLOC(sizeof(_TLInt));
  i->type = TL_INT;
  i->value = atoll(s);
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
    assert(false);
    break;
  case TOK_NOTHING:
    break;
  case TOK_COMMA:
    out->toks[out->toksi++] = (Token){TOK_COMMA, 0, 0, 0};
    break;
  case TOK_INT:
    out->toks[out->toksi++] = (Token){TOK_INT,int_const_from(TL, btok),0};
    break;
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
// #define HASH_CASE_C(NAME) 
//             case (HASH_##NAME) : current = TOK_##NAME ; break;
// #define HASH_CASE_S(NAME) 
//             case (HASH_##NAME) : 
//             current = TOK_##NAME ; stop_tokenizing = true; break;
        case HASH_FN: *current = TOK_FN; break;
        case HASH_RETURN: *current = TOK_RETURN; break;
        case HASH_IF: *current = TOK_IF; stop_tokenizing = true; break; 
        case HASH_ELIF: *current = TOK_ELIF; stop_tokenizing = true; break; 
        case HASH_ELSE: *current = TOK_ELSE; stop_tokenizing = true; break; 
        case HASH_WHILE: *current = TOK_WHILE; stop_tokenizing = true; break; 
        case HASH_DO: *current = TOK_DO; stop_tokenizing = true; break; 
        case HASH_THEN: *current = TOK_THEN; stop_tokenizing = true; break; 
        case HASH_BREAK: *current = TOK_BREAK; stop_tokenizing = true; break; 
        case HASH_CONTINUE: *current = TOK_CONTINUE; stop_tokenizing = true; break; 
        case HASH_END: *current = TOK_END; stop_tokenizing = true; break; 
// #undef HASH_CASE
      }
      if (*current >= TOK_FN)
      {
        out->toks[out->toksi++] = (Token){*current,0,0};
      }
      else
      {
        
        const int i = tl_get_name(TL->scope, btok)->global_str_idx;
        out->toks[out->toksi++] = (Token){TOK_NAME, i, 0};
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
        assert(j < out->toksi);
      }
      else
      {
        (*depth)++;
        out->toks[out->toksi++] = (Token){*current, UNSET_PARENTH, 0};
        if (out->max_depth < *depth)
          out->max_depth = *depth;
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
  out.max_depth = out.max_prec = out.toksi = 0;
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
    // TODO: LISTS
    
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

static inline bool tok_is_brace(Token tok)
{ return tok.type == TOK_PARENTH || tok.type == TOK_BRACKET; }
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

int tl_expr_to_rpn(
  TLState* G,
  TokenizerOut* toks,
  int start, int len)
{
  // printf("[%i to %i]\n", start, start+len);
  Token* T = toks->toks+start;
  const int initial_len = len;
  BraceManager bm;
  for (int depth = toks->max_depth; depth >= 0; depth--)
  {
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
        printf("BEFORE: (depth=%i/%i, prec=%i/%i)\n",
               depth, toks->max_depth,
               prec, toks->max_prec);
        tl_debug_token_array(G, T, len, i);
        update_brace_manager(&bm, T, T[i], i);
        if (tok_is_brace(T[i]) && depth-1 == current_depth(&bm))
        {
          if (i > 0
              && (
                (
                   T[i-1].type != TOK_OP
                && T[i-1].type != TOK_PARENTH
                && T[i-1].type != TOK_BRACKET
                )
              || T[i-1].rpned > 0
              )
          ) {
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
            toks->max_depth -= depth;
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
                && depth-1 == current_depth(&bm)
                ) {
                  const int rdux = tl_expr_to_rpn(
                     G, toks,
                     i+param_start+1 + start,
                     param_tok-param_start
                  );
                  len -= rdux;
                  T[i].arg -= rdux;
                  param_start = param_tok;
                  argc++;
                }
              }
              const int rdux = tl_expr_to_rpn(
                 G, toks,
                 i+param_start+1 + start,
                 T[i].arg-param_start
              );
              len -= rdux;
              T[i].arg -= rdux;
            }
            toks->max_depth += depth;
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
          else if (T[i].type == TOK_PARENTH)
          {
            toks->max_depth -= depth;
            const int rdux = tl_expr_to_rpn(G, toks, i+1+start, T[i].arg);
            len -= rdux;
            if (T[i+1].rpned)
            {
              T[i].rpned = T[i+1].rpned+1;
              T[i+T[i].rpned-1].rpned = T[i].rpned;
            }
            toks->max_depth += depth;
          }
          else if (T[i].type == TOK_BRACKET)
          {
            if (T[i].arg == 0)
            {
              T[i].rpned = 1;
              continue;
            }
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
              && depth-1 == current_depth(&bm)
              ) {
                if (T[i].arg == list_len)
                  T[i].arg = j - elem_start;
                else
                {
                  T[i+elem_start].type = TOK_LC;
                  T[i+elem_start].arg = j - elem_start;
                }
                const int rdux = tl_expr_to_rpn(
                   G, toks,
                   i+elem_start+1 + start,
                   j-elem_start
                );
                len -= rdux;
                elem_start = j+1;
              }
            }
            if (T[i].arg != list_len)
            {
              T[i+elem_start].type = TOK_LC;
              T[i+elem_start].arg = list_len - elem_start;
            }
            const int rdux = tl_expr_to_rpn(
               G, toks,
               i+elem_start+1 + start,
               list_len-elem_start
            );
            T[i].rpned = list_len+1;
            T[i].op_argc = list_len;
            T[i+T[i].rpned-1].rpned = list_len+1;
            len -= rdux;
          }
          else
            assert(false);
        }
        else if (tok_is_brace(T[i]))
          push_brace(&bm, T[i], i);
        else if (depth == current_depth(&bm))
        {
          const TLName* name = tl_has_name_ex(G->scope, T[i].arg);
          tl_object_t* op = name ? name->data : NULL;
          if (T[i].type == TOK_OP && tl_type_of_pro(op) == TL_NIL)
          {
            fprintf(
              stderr,
              "ERROR: Use of undeclared operator '%s'.\n",
              tl_to_str_pro(G->consts[T[i].arg])
            );
            return 0;
          }
          if (
             T[i].type == TOK_OP
          && tl_precedence_pro(op) == prec
          ) {
            if (i == 0 || (T[i-1].rpned == 0 && T[i-1].type == TOK_OP))
            {
              assert(len > 1);
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
              assert(len > 2);
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
        printf("AFTER: (depth=%i/%i, prec=%i/%i)\n",
               depth, toks->max_depth,
               prec, toks->max_prec);
        tl_debug_token_array(G, T, len, i);
      }
    }
  }
  return initial_len - len;
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
      case TOK_NOTHING: printf("(/) "); break;
      case TOK_FN: printf("fn "); break;
      case TOK_CALL: printf("<call> "); break;
      case TOK_DO: printf("do "); break;
      case TOK_ELSE: printf("else "); break;
      case TOK_END: printf("end "); break;
      case TOK_THEN: printf("then "); break;
      case TOK_WHILE: printf("while "); break;
      case TOK_RETURN: printf("return "); break;
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
    else
    {
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
#define MAX_FUNC 16
  TokenizerState TS;
  // int func_depths_stack[MAX_FUNC];
  // int func_depths_sp = 0;
  int cond_depth = 0;
  TS.i = 0;
  TS.len = _tl_str_len(code);
  while (TS.i < TS.len)
  {
    const int _prev_i = TS.i;
    TokenizerOut toks = tl_tokenize_string(TL, &TS, code);
    // puts("STOP");
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
      assert(toks.toksi >= 3);
      uint32_t prec = 255;
      tl_object_t obj;
      int argc = 0;
      int offset = 0;
      if (toks.toks[1].type == TOK_INT)
      {
        // 'fn' <prec> <func_name> '(' ...
        assert(toks.toks[2].type == TOK_NAME
            || toks.toks[2].type == TOK_OP);
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
      assert(1+offset+name_len < toks.toksi); // MISSING PARENTHESIS
      const int redux = tl_expr_to_rpn(TL, &toks, 1+offset, name_len);
      name_len -= redux;
      toks.toksi -= redux;
      tl_rpn_to_bytecode(TL, &toks, 1+offset, name_len);
      offset += name_len;
      _bc_push(TL, CODE(TLOP_FN, 0));
      for (int i = 3+offset; i < toks.toksi; i++)
      {
        if (toks.toks[i].type == TOK_COMMA)
          continue;
        assert(toks.toks[i].type == TOK_NAME);
        _bc_push(TL, CODE(TLOP_NEW_NAME, toks.toks[i].arg));
        _bc_push(TL, CODE(TLOP_PARAM, argc));
        argc++;
      }

      if (toks.toks[2].type == TOK_OP)
        assert(argc == 1 || argc == 2);
        // operators can only have 1 or 2 arguments

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
      f->bytecode_pt = TL->bc_len - argc*2;
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
        const int redux = tl_expr_to_rpn(TL, &toks, start, i-start);
        tl_rpn_to_bytecode(TL, &toks, start, i-start-redux);
        i++;
      }
      _bc_push(TL, CODE(TLOP_RET, ret_count));
    }
    else
    {
      const int redux = tl_expr_to_rpn(TL, &toks, 0, toks.toksi);
      tl_rpn_to_bytecode(TL, &toks, 0, toks.toksi-redux);
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


// test.c
#include "tl.h"
#include <stdio.h>


int main(int argc, const char* argv[])
{
  if (argc > 2)
  {
    puts("Syntax: toylang [script.tl]\n");
    return 0;
  }
  else if (argc == 1)
  {
    // Interactive toylang shell
    puts("TODO");
    return 0;
  }
  else
  {
    FILE* f = fopen(argv[1], "r");
    if (!f)
    {
      fprintf(stderr, "ERROR: Failed to open file '%s'\n", argv[1]);
      return 1;
    }
    char* buf;
    fseek(f, 0, SEEK_END);
    const long len = ftell(f);
    buf = calloc(len+1, sizeof(char));
    fseek(f, 0, SEEK_SET);
    fread(buf, sizeof(char), len, f);
    fclose(f);
    
    TLState* TL = tl_new_state();
    tl_load_openlib(TL);
    tl_parse_to_bytecode(TL, buf);
    tl_fdebug_state(stdout, TL);
    tl_run_bytecode(TL);
    tl_destroy(TL);
    free(buf);
  }
  return 0;
}


// tlopenlib.c
#include "tl.h"
#include <string.h>

int _tl_op_call(TLScope *S, int argc)
{
  tl_pop(S);
  return tl_call(S, argc-1);
}

int op_eq(TLScope* TL, int argc)
{
  if (argc != 2)
  {
    fprintf(stderr, "ERROR: Operator '=' needs 2 arguments, not %i\n", argc);
    tl_push_nil(TL);
    return 1;
  }
  tl_pop(TL); // pop function
  tl_object_t src = tl_get(TL, -1);
  TLData* dst = &TL->stack[TL->stack_top-2];
  TLName* n = dst->ref;
  _tl_drop(TL->global, n->data);
  n->data = src;
  _tl_hold(TL->global, src);
  tl_pop(TL);
  return 1;
}


#define OPI2H(NAME, OP, STR_COMPUTE) \
int NAME(TLScope* TL, int argc) \
{ \
  if (argc != 2) \
  { \
    fprintf(stderr, "ERROR: Operator '" #OP "' needs 2 arguments, not %i\n", argc); \
    tl_push_nil(TL); \
    return 1; \
  } \
  tl_pop(TL); \
  tl_object_t a = tl_get(TL, -2); \
  tl_object_t b = tl_get(TL, -1); \
  const TLType Ta = tl_type_of_pro(a); \
  const TLType Tb = tl_type_of_pro(b); \
  if (Ta == TL_FLOAT || Tb == TL_FLOAT) \
  { \
    assert(Ta == TL_INT || Ta == TL_FLOAT); \
    assert(Tb == TL_INT || Tb == TL_FLOAT); \
    double va, vb; \
    if (Ta == TL_FLOAT) \
    { \
      va = tl_to_float_pro(a); \
    } \
    else \
    { \
      assert(Ta == TL_INT); \
      va = tl_to_int_pro(a); \
    } \
    if (Tb == TL_FLOAT) \
      vb = tl_to_float_pro(b); \
    else \
      vb = tl_to_int_pro(b); \
    tl_pop(TL); \
    tl_pop(TL); \
    tl_push_float(TL, va OP vb); \
  } \
  else if (Ta == TL_INT) \
  { \
    if (Tb != TL_INT) \
    { \
      tl_push_type_name(TL, Tb); \
      fprintf(stderr, "ERROR: Unknown builtin operation " #OP " between an int and a %s\n", \
              tl_to_str_pro(TL)); \
      tl_push_nil(TL); \
      return 1; \
    } \
    int64_t r = tl_to_int_pro(a) OP tl_to_int_pro(b); \
    tl_pop(TL); \
    tl_pop(TL); \
    tl_push_int(TL, r); \
  } \
  else if (Ta == TL_STR) \
  STR_COMPUTE \
  else \
  { \
    tl_push_type_name(TL, Ta); \
    const char* Tas = tl_to_str_pro(TL); \
    tl_push_type_name(TL, Tb); \
    const char* Tbs = tl_to_str_pro(TL); \
    fprintf(stderr, "ERROR: Unknown builtin operation (%s " #OP " %s)\n", \
            Tas, Tbs); \
    assert(false); \
  } \
  return 1; \
}

#define CMPI2H(NAME, OP, STR_COMPUTE) \
int NAME(TLScope* TL, int argc) \
{ \
  if (argc != 2) \
  { \
    fprintf(stderr, "ERROR: Operator '" #OP "' needs 2 arguments, not %i\n", argc); \
    tl_push_nil(TL); \
    return 1; \
  } \
  tl_pop(TL); \
  tl_object_t a = tl_get(TL, -2); \
  tl_object_t b = tl_get(TL, -1); \
  const TLType Ta = tl_type_of_pro(a); \
  const TLType Tb = tl_type_of_pro(b); \
  if (Ta == TL_FLOAT || Tb == TL_FLOAT) \
  { \
    assert(Ta == TL_INT || Ta == TL_FLOAT); \
    assert(Tb == TL_INT || Tb == TL_FLOAT); \
    double va, vb; \
    if (Ta == TL_FLOAT) \
    { \
      va = tl_to_float_pro(a); \
    } \
    else \
    { \
      assert(Ta == TL_INT); \
      va = tl_to_int_pro(a); \
    } \
    if (Tb == TL_FLOAT) \
      vb = tl_to_float_pro(b); \
    else \
      vb = tl_to_int_pro(b); \
    tl_pop(TL); \
    tl_pop(TL); \
    tl_push_bool(TL, va OP vb); \
  } \
  else if (Ta == TL_INT) \
  { \
    if (Tb != TL_INT) \
    { \
      tl_push_type_name(TL, Tb); \
      fprintf(stderr, "ERROR: Unknown builtin operation " #OP " between an int and a %s\n", \
              tl_to_str_pro(TL)); \
      tl_push_nil(TL); \
      return 1; \
    } \
    bool r = tl_to_int_pro(a) OP tl_to_int_pro(b); \
    tl_pop(TL); \
    tl_pop(TL); \
    tl_push_bool(TL, r); \
  } \
  else if (Ta == TL_STR) \
  STR_COMPUTE \
  else \
  { \
    tl_push_type_name(TL, Ta); \
    const char* Tas = tl_to_str_pro(TL); \
    tl_push_type_name(TL, Tb); \
    const char* Tbs = tl_to_str_pro(TL); \
    fprintf(stderr, "ERROR: Unknown builtin operation (%s " #OP " %s)\n", \
            Tas, Tbs); \
    assert(false); \
  } \
  return 1; \
}

OPI2H(op_add, +, {
if (Tb != TL_STR)
{
  tl_push_type_name(TL, Tb);
  fprintf(stderr, "ERROR: Builtins do not allow sum of str and %s.\n",
    tl_to_str_pro(TL));
  assert(false);
}
const char* sa = tl_to_str_pro(a);
const char* sb = tl_to_str_pro(b);
const int la = _tl_str_len(sa);
const int lb = _tl_str_len(sb);
_TLStr* out = TL_MALLOC(sizeof(_TLStr) + sizeof(char)*(la+lb+1));
out->owned = true;
out->type = TL_STR;
char* p = (char*)(out+1);
memcpy(p, sa, la);
memcpy(p+la, sb, lb+1);
tl_pop(TL);
tl_pop(TL);
tl_push(TL, out);
return 1;
})

OPI2H(op_sub, -, {
fprintf(stderr, "ERROR: Builtins do not allow substraction from str.\n");
assert(false);
})

OPI2H(op_mul, *, {
assert(false);
})

CMPI2H(cmp_eq, ==,
{
  if (Tb != TL_STR)
    return false;
  const char* sa = tl_to_str_pro(a);
  const char* sb = tl_to_str_pro(b);
  bool out = _tl_str_eq(sa, sb);
  tl_pop(TL);
  tl_pop(TL);
  tl_push_bool(TL, out);
  return 1;
})

CMPI2H(cmp_neq, !=,
{
  if (Tb != TL_STR)
    return false;
  const char* sa = tl_to_str_pro(a);
  const char* sb = tl_to_str_pro(b);
  bool out = !_tl_str_eq(sa, sb);
  tl_pop(TL);
  tl_pop(TL);
  tl_push_bool(TL, out);
  return 1;
})

CMPI2H(cmp_lt, <,
{
  if (Tb != TL_STR)
    return false;
  const char* sa = tl_to_str_pro(a);
  const char* sb = tl_to_str_pro(b);
  bool out = strcmp(sa, sb) == -1;
  tl_pop(TL);
  tl_pop(TL);
  tl_push_bool(TL, out);
  return 1;
})

CMPI2H(cmp_gt, >,
{
  if (Tb != TL_STR)
    return false;
  const char* sa = tl_to_str_pro(a);
  const char* sb = tl_to_str_pro(b);
  bool out = strcmp(sa, sb) == 1;
  tl_pop(TL);
  tl_pop(TL);
  tl_push_bool(TL, out);
  return 1;
})

CMPI2H(cmp_le, <=,
{
  if (Tb != TL_STR)
    return false;
  const char* sa = tl_to_str_pro(a);
  const char* sb = tl_to_str_pro(b);
  bool out = strcmp(sa, sb) <= 0;
  tl_pop(TL);
  tl_pop(TL);
  tl_push_bool(TL, out);
  return 1;
})

CMPI2H(cmp_ge, >=,
{
  if (Tb != TL_STR)
    return false;
  const char* sa = tl_to_str_pro(a);
  const char* sb = tl_to_str_pro(b);
  bool out = strcmp(sa, sb) >= 0;
  tl_pop(TL);
  tl_pop(TL);
  tl_push_bool(TL, out);
  return 1;
})

int op_dot(TLScope* S, int argc)
{
  if (argc != 2)
  {
    fprintf(stderr, "ERROR: Operator '.' needs 2 arguments, not %i\n", argc);
    tl_push_nil(S);
    return 1;
  }
  tl_pop(S);
  const tl_object_t a = tl_get(S, -2);
  const TLType Ta = tl_type_of_pro(a);
  if (Ta == TL_SCOPE)
  {
    TLScope* const scope = tl_to_scope_pro(a);
    const TLData* b = tl_top_ex(S);
    if (b->ref)
    { // a.b (attribute access)
      const char* attr_name =
        tl_to_str_pro(S->global->consts[b->ref->global_str_idx]);
      TLName* const child = tl_get_local(scope, attr_name);
      tl_push(S, child->data);
      tl_top_ex(S)->ref = child;
      return 1;
    }
    else
    { // a.0 (constant indexing)
      char _buf[16];
      assert(tl_type_of_pro(b->object) == TL_INT);
      const int64_t index =
        tl_to_int_pro(b->object);
      assert(index >= 0);
      _tl_idstr_from_int(_buf, index, sizeof(_buf));
      TLName* const child = tl_get_local(scope, _buf);
      tl_push(S, child->data);
      tl_top_ex(S)->ref = child;
      return 1;
    }
  }
  else
  {
    tl_push_type_name(S, Ta);
    const char* Tas = tl_to_str(S);
    TLName* meta = tl_get_name(S, Tas);
    if (tl_type_of_pro(meta->data) != TL_SCOPE)
    {
      fprintf(stdout,
              "ERROR: %s doesn't have a proper meta-scope.",
              Tas
      );
      return 0;
    }
  }
  return 0;
}

int scope(TLScope* TL, int argc)
{
  assert(argc == 0);
  tl_pop(TL);
  _TLScope* virt_child = TL_MALLOC(sizeof(_TLScope));
  virt_child->type = TL_SCOPE;
  virt_child->scope = tl_new_scope_ex(
    TL->global,
    TL,
    TL->stack_cap,
    TL->max_name_count,
    TL->bc_pos
  );
  tl_push(TL, virt_child);
  return 1;
}

int print(TLScope* TL, int argc)
{
  if (argc == 0)
  {
    puts("");
    return 0;
  }
  tl_pop(TL); // pop funcion
  tl_object_t o = tl_get(TL, -argc);
  if (tl_type_of_pro(o) == TL_STR)
    printf("%s", tl_to_str_pro(o));
  else
    tl_fdebug_obj(stdout, o);
  for (int i = 1; i < argc; i++)
  {
    o = tl_get(TL, i-argc);
    printf("\t");
    if (tl_type_of_pro(o) == TL_STR)
      printf("%s", tl_to_str_pro(o));
    else
      tl_fdebug_obj(stdout, o);      
  }
  tl_pop(TL);
  puts("");
  return 0;
}

int _debug_state(TLScope* TL, int argc)
{
  tl_fdebug_state(stdout, TL->global);
  return 0;
}

int _debug_scope(TLScope* TL, int argc)
{
  tl_pop(TL);
  if (argc == 1)
    tl_fdebug_scope(stdout, tl_to_scope_pro(tl_top(TL)));
  else
    tl_fdebug_scope(stdout, TL);
  return 0;
}


void tl_load_openlib(TLState* TL)
{
  tl_register_func(TL, &_tl_op_call, "()", 999999, 15);
  tl_register_func(TL, &op_eq, "=", 2, 1);
  tl_register_func(TL, &op_add, "+", 2, 4);
  tl_register_func(TL, &op_sub, "-", 2, 4);
  tl_register_func(TL, &op_mul, "*", 2, 5);
  tl_register_func(TL, &op_dot, ".", 2, 6);
  tl_register_func(TL, &cmp_eq, "==", 2, 3);
  tl_register_func(TL, &cmp_neq, "!=", 2, 3);
  tl_register_func(TL, &cmp_lt, "<", 2, 3);
  tl_register_func(TL, &cmp_gt, ">", 2, 3);
  tl_register_func(TL, &cmp_le, "<=", 2, 3);
  tl_register_func(TL, &cmp_ge, ">=", 2, 3);
  tl_register_func(TL, &print, "print", 2, 255);
  tl_register_func(TL, &scope, "scope", 0, 255);
  tl_register_func(TL, &_debug_state, "_debug_state", 0, 255);
  tl_register_func(TL, &_debug_scope, "_debug_scope", 1, 255);
}




// tl.c
#include "tl.h"
#include <assert.h>
#include <ctype.h>
#include <ctype.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int _tl_str_len(const char* const s)
{
  if (s == NULL || *s == 0)
    return 0;
  if (*s == '@')
    return 7;
  return strlen(s);
}

bool _tl_str_eq(const char* s1, const char* s2)
{
  if (s1 == s2)
    return true;
  if (s1 == NULL || s2 == NULL)
    return false;
  if (*s1 == 0 || *s2 == 0)
    return *s1 == *s2;
  if (*s1 == '@' || *s2 == '@')
  {
    if (*s1 != *s2) return false;
    return *(uint64_t*)s1 == *(uint64_t*)s2;
  }
  while (*s1 != 0 && *s2 != 0)
  {
    if (*s1 != *s2)
      return false;
    s1++; s2++;
  }
  return *s1 == *s2;
}

void _tl_idstr_from_int(char* buf, int64_t val, int n)
{
  assert(n >= 8);
  assert(val >= 0);
  val <<= 8;
  memcpy(buf, &val, sizeof(int64_t));
  buf[0] = '@';
}

const char* _tl_fmt_name(const char* s)
{
  static char buf[16];
  if (*s == '@')
  {
    const uint64_t i = *(uint64_t*)s;
    snprintf(buf, 16, "@%lu", (i >> 8));
    return buf;
  }
  return s;
}

const char* tl_name_to_str(TLScope* S, int idx)
{
  assert(S->names[idx].global_str_idx >= 0);
  assert(S->names[idx].global_str_idx < S->global->consts_len);
  const char* s = tl_to_str_pro(S->global->consts[S->names[idx].global_str_idx]);
  return s;
}

// Call function on top of the stack
// Returns the amount of returned values
int tl_call(TLScope* S, int argc)
{
  const int old_stack_top = S->stack_top;
  const TLType func_type = tl_type_of(S);
  const int prearg_stack_top = old_stack_top - argc - 1;
  int retc = 0;
  if (func_type == TL_NIL)
  {
    fprintf(stderr, "ERROR: Calling a nil value.\n");
    return 0;
  }
  else if (func_type == TL_FUNC)
  {
    const _TLFunc* f = (_TLFunc*)tl_top(S);
    TLScope* child = tl_new_scope_ex(
      S->global, S, S->stack_cap, S->max_name_count, f->bytecode_pt
    );
    retc = tl_run_bytecode_ex(
      child, S->global->bc_len, 1, argc
    );
    const int offset = child->stack_top - retc;
    for (int i = 0; i < retc; i++)
    {
      S->stack[prearg_stack_top+i] = child->stack[i+offset];
      _tl_hold(S->global, S->stack[prearg_stack_top+i].object);
      if (S->stack[prearg_stack_top+i].ref)
      {
        if (
           child->names <= S->stack[prearg_stack_top+i].ref
        && child->names+child->name_count > S->stack[prearg_stack_top+i].ref
        )
          S->stack[prearg_stack_top+i].ref = NULL;
      }
    }
    S->stack_top = prearg_stack_top + retc;
    tl_destroy_scope(child);
  }
  else if (func_type == TL_CFUNC)
  {
    const _TLCFunc* f = (_TLCFunc*)tl_top(S);
    retc = f->ptr(S, argc);
    const int offset = S->stack_top - retc;
    for (int i = 0; i < retc; i++)
    {
      S->stack[i+prearg_stack_top] = S->stack[i+offset];
    }
    S->stack_top = prearg_stack_top + retc;
  }
  else
  {
    tl_push_type_name(S, func_type);
    fprintf(stderr,
      "ERROR: %s is not callable by default.",
      tl_to_str(S)
    );
    tl_pop(S);
    return 0;
  }
  if (S->stack_top < 0)
    S->stack_top = 0;
  return retc;
}

void tl_register_func(
  TLState* TL, tl_cfunc_ptr_t f,
  const char* name, int max_arg, int prec
) {
  _TLCFunc* tl_f = (_TLCFunc*)TL_MALLOC(sizeof(_TLCFunc));
  tl_f->argc = max_arg;
  tl_f->prec = prec;
  tl_f->ptr = f;
  tl_f->type = TL_CFUNC;
  _tl_consts_push(TL, tl_f);
  tl_set_name(TL->scope, name, tl_f);
}

TLName* tl_get_name_ex(TLScope *TL, int global_stack_str_index)
{
  TLScope* org = TL;
  do
  {
    for (int i = 0; i < TL->name_count; i++)
    {
      if (TL->names[i].global_str_idx == global_stack_str_index)
        return TL->names + i;
    }
    if (TL->parent == NULL)
      break;
    TL = TL->parent;
  } while(true);
  const int i = tl_set_name_ex(org, global_stack_str_index, NULL);
  return org->names + i;
}

TLName* tl_get_name(TLScope *TL, const char* name)
{
  TLScope* org = TL;
  do
  {
    for (int i = 0; i < TL->name_count; i++)
    {
      if (_tl_str_eq(tl_name_to_str(TL, i), name))
        return TL->names + i;
    }
    if (TL->parent == NULL)
      break;
    TL = TL->parent;
  } while(true);
  const int i = tl_set_name(org, name, NULL);
  return org->names + i;
}
TLName* tl_get_local(TLScope* S, const char* s)
{
  for (int i = 0; i < S->name_count; i++)
  {
    if (_tl_str_eq(tl_name_to_str(S, i), s))
      return S->names + i;
  }
  const int i = tl_set_name(S, s, NULL);
  return S->names + i;
}

TLName* tl_has_name_ex(TLScope *TL, int global_stack_str_index)
{
  do
  {
    for (int i = 0; i < TL->name_count; i++)
    {
      if (TL->names[i].global_str_idx == global_stack_str_index)
        return TL->names + i;
    }
    if (TL->parent == NULL)
      break;
    TL = TL->parent;
  } while(true);
  return NULL;
}

TLState* tl_new_state()
{
  TLState* TL = (TLState*)TL_MALLOC(sizeof(TLState));
  TL->bc = NULL;
  TL->consts = NULL;
  TL->gc_data = NULL;
  TL->gc_ref_counts = NULL;
  TL->consts_cap = TL->consts_len = TL->bc_cap = TL->bc_len
    = TL->gc_cap = TL->gc_len = 0;
  TL->scope = tl_new_scope_ex(
    TL, NULL, DEFAULT_STACK_CAP,
    DEFAULT_MAX_NAME_COUNT, -1
  );
  return TL;
}

TLScope* tl_new_scope_ex(
  TLState* state,
  TLScope* parent,
  int mem_size, int max_name_count,
  int child_bcpos
) {
  TLScope* self = TL_MALLOC(sizeof(TLScope));
  if (parent == NULL)
  {
    assert(child_bcpos == -1);
    self->bc_pos = 0;
  }
  else
  {
    self->bc_pos = child_bcpos;
  }
  self->global = state;
  self->parent = parent;
  self->stack = TL_MALLOC(mem_size*sizeof(TLData));
  self->names = TL_MALLOC(max_name_count*sizeof(TLName));
  memset(self->stack, 0, mem_size*sizeof(TLData));
  memset(self->names, 0, max_name_count*sizeof(TLName));
  self->stack_cap = mem_size;
  self->stack_top = 0;
  self->max_name_count = max_name_count;
  self->name_count = 0;
  return self;
}

void tl_destroy_custom(TLScope* S, tl_object_t custom)
{
  tl_push(S, custom);
  tl_custom_info(S, TL_MSG_DESTROY);
}

void tl_destroy_object(TLScope* S, tl_object_t obj)
{
  const TLType type = tl_type_of_pro(obj);
  if (type == TL_SCOPE)
  {
    tl_destroy_scope(((_TLScope*)obj)->scope);
    TL_FREE(obj);
  }
  else if (type == TL_CUSTOM)
    tl_destroy_custom(S, obj);
  else
    TL_FREE(obj);
}

void tl_destroy_scope(TLScope* S)
{
  for (int i = 0; i < S->stack_top; i++)
  {
    _tl_drop(S->global, S->stack[i].object);
  }
  TL_FREE(S->stack);
  for (int i = 0; i < S->name_count; i++)
  {
    _tl_drop(S->global, S->names[i].data);
  }
  TL_FREE(S->names);
  S->stack_cap = S->stack_top = S->name_count = S->max_name_count = 0;
  TL_FREE(S);
}

void tl_destroy(TLState* TL)
{
  for (int i = 0; i < TL->gc_len; i++)
  {
    tl_destroy_object(TL->scope, TL->gc_data[i]);
    TL->gc_data[i] = NULL;
    TL->gc_ref_counts[i] = 0;
  }
  TL_FREE(TL->gc_ref_counts);
  TL_FREE(TL->gc_data);
  TL->gc_cap = TL->gc_len = 0;
  TL_FREE(TL->consts);
  TL->consts_cap = TL->consts_len = 0;
  TL_FREE(TL->bc);
  TL->bc_len = TL->bc_cap = 0;
  tl_destroy_scope(TL->scope);
  TL->scope = NULL;
  TL_FREE(TL);
}

void tl_do_file(TLState* TL, FILE* f)
{
  assert(f != NULL);
  fseek(f, 0, SEEK_END);
  const long l = ftell(f);
  fseek(f, 0, SEEK_SET);
  char* code = TL_MALLOC(l+1);
  fread(code, l, 1, f);
  code[l] = 0;
  tl_do_string(TL, code);
  TL_FREE(code);
}

void tl_pop(TLScope* S)
{
  if (S->stack_top == 0)
    return;
  S->stack_top--;
  _tl_drop(S->global, S->stack[S->stack_top].object);
}

TLType tl_type_of_pro(tl_object_t obj)
{
  if (obj == NULL)
    return TL_NIL;
  return *(TLType*)obj;
}

void tl_push_type_name(TLScope* S, TLType type)
{
  const char* TABLE[TL_CUSTOM+1] = {
    "nil", "int", "float", "bool", "str", "function", "cfunction",
    "scope", "custom"
  };
  if (type >= TL_CUSTOM)
    tl_custom_info(S, TL_MSG_NAME);
  else
    tl_push_rstr(S, TABLE[type]);
}

uint64_t tl_size_of_pro(TLScope* S, tl_object_t obj)
{
  const TLType type = tl_type_of_pro(obj);
  switch (type)
  {
    case TL_NIL: return 0;
    case TL_INT: return sizeof(_TLInt);
    case TL_FLOAT: return sizeof(_TLFloat);
    case TL_STR: return sizeof(_TLStr);
    case TL_BOOL: return sizeof(_TLBool);
    case TL_CFUNC: return sizeof(_TLCFunc);
    case TL_SCOPE: return sizeof(_TLScope);
    case TL_CUSTOM:
      perror("ERROR: Custom objects not supported yet.");
      abort();
      return 0;
    default:
      fprintf(stderr, "ERROR: Unknown type with id: %i\n", (int)type);
      abort();
      return 0;
  }
}

bool tl_to_bool(TLScope* S)
{
  const int type = tl_type_of(S);
  switch (type)
  {
    case TL_INT: return tl_to_int(S) != 0;
    case TL_FLOAT: return tl_to_float(S) != 0;
    case TL_CFUNC:
    case TL_FUNC:
    case TL_CUSTOM:
      return true;
    case TL_NIL:
      return false;
    case TL_BOOL:
      return ((_TLBool*)tl_top(S))->value;
    case TL_STR:
      {
      const char* s = tl_to_str(S);
      return !(s == NULL || *s == 0);
      }
    case TL_SCOPE:
      {
      const TLScope* sc = tl_to_scope_pro(tl_top(S));
      return sc->stack_top > 0 || sc->name_count > 0;
      }
  }
  return false;
}


void _tl_hold(TLState* TL, tl_object_t obj)
{
  if (obj == NULL)
    return;
  int null_found = -1;
  for (int i = 0; i < TL->gc_len; i++)
  {
    if (TL->gc_data[i] == obj)
    {
      TL->gc_ref_counts[i]++;
      return;
    }
    else if (TL->gc_data[i] == NULL)
    {
      null_found = i;
    }
  }
  if (null_found != -1)
  {
    TL->gc_data[null_found] = obj;
    TL->gc_ref_counts[null_found] = 1;
    return;
  }
  TL->gc_len++;
  if (TL->gc_len > TL->gc_cap)
  {
    if (TL->gc_data == NULL)
    {
      assert(TL->gc_ref_counts == NULL);
      TL->gc_data = TL_MALLOC(sizeof(tl_object_t));
      TL->gc_ref_counts = TL_MALLOC(sizeof(typeof(*TL->gc_ref_counts)));
      TL->gc_cap = 1;
    }
    else
    {
      TL->gc_cap = TL->gc_cap+TL->gc_cap/2+(TL->gc_cap<2);
      TL->gc_data = TL_REALLOC(TL->gc_data, sizeof(tl_object_t)*TL->gc_cap);
      TL->gc_ref_counts = TL_REALLOC(
        TL->gc_ref_counts,
        sizeof(typeof(*TL->gc_ref_counts))*TL->gc_cap
      );
    }
  }
  TL->gc_data[TL->gc_len-1] = obj;
  TL->gc_ref_counts[TL->gc_len-1] = 1;
}

void _tl_drop(TLState* TL, tl_object_t obj)
{
  if (obj == NULL)
    return;
#define GC_DISPLACE_THRESHOLD 64
  int first_del = -1;
  int i = 0;
  int count = 0;
  for (; i < TL->gc_len; i++)
  {
    if (TL->gc_data[i] == NULL)
    {
      count++;
      if (first_del == -1)
        first_del = i;
    }
    if (TL->gc_data[i] == obj)
    {
      TL->gc_ref_counts[i]--;
      if (TL->gc_ref_counts[i] > 0)
        return;
      tl_destroy_object(TL->scope, TL->gc_data[i]);
      TL->gc_data[i] = NULL;
      count++;
      if (first_del == -1)
        first_del = i;
    }
  }
  if (count >= GC_DISPLACE_THRESHOLD)
  {
    TL->gc_len -= count;
    i = first_del;
    first_del = 0;
    for (; i < TL->gc_len; i++)
    {
      while (TL->gc_ref_counts[i+first_del] == 0)
        first_del++;
      TL->gc_data[i] = TL->gc_data[i+first_del];
      TL->gc_ref_counts[i] = TL->gc_ref_counts[i+first_del];
    }
  }
#undef GC_DISPLACE_THRESHOLD
}

int _tl_consts_push(TLState* TL, tl_object_t obj)
{
  TL->consts_len++;
  if (TL->consts_len > TL->consts_cap)
  {
    if (TL->consts == NULL)
    {
      TL->consts = TL_MALLOC(sizeof(tl_object_t));
      TL->consts_cap = 1;
    }
    else
    {
      TL->consts_cap += TL->consts_cap/2 + (TL->consts_cap<2);
      TL->consts = TL_REALLOC(TL->consts, sizeof(tl_object_t)*TL->consts_cap);
    }
  }
  _tl_hold(TL, obj);
  TL->consts[TL->consts_len-1] = obj;
  return TL->consts_len-1;
}

int _tl_consts_push_str(TLState* TL, const char* s)
{
  const int l = _tl_str_len(s);
  _TLStr* i = TL_MALLOC(sizeof(_TLStr) + l+1);
  i->type = TL_STR;
  i->owned = true;
  memcpy(i+1, s, l+1);
  const int r = _tl_consts_push(TL, i);
  return r;
}

int tl_set_name_ex(TLScope* S, int name_str_global, tl_object_t object)
{
  assert(S->name_count < S->max_name_count);
  for (int i = 0; i < S->name_count; i++)
  {
    if (S->names[i].global_str_idx == name_str_global)
    {
      S->names[i].data = object;
      return i;
    }
  }
  S->names[S->name_count].global_str_idx = name_str_global;
  S->names[S->name_count++].data = object;
  return S->name_count-1;
}

int tl_set_name(TLScope* S, const char* name, tl_object_t object)
{
  assert(S->name_count < S->max_name_count);
  for (int i = 0; i < S->name_count; i++)
  {
    if (_tl_str_eq(tl_name_to_str(S, i), name))
    {
      _tl_drop(S->global, S->names[i].data);
      S->names[i].data = object;
      _tl_hold(S->global, object);
      return i;
    }
  }
  S->names[S->name_count].global_str_idx =
    _tl_consts_push_str(S->global, name);
  S->names[S->name_count++].data = object;
  return S->name_count-1;
}


int tl_push_int_ex(TLScope* S, int64_t value)
{
  assert(S->stack_top < S->stack_cap);
  _TLInt* i = TL_MALLOC(sizeof(_TLInt));
  _tl_hold(S->global, i);
  i->type = TL_INT;
  i->value = value;
  S->stack[S->stack_top].ref = NULL;
  S->stack[S->stack_top++].object = i;
  return S->stack_top-1;
}

int tl_push_str_ex(TLScope* TL, const char *value)
{
  assert(TL->stack_top < TL->stack_cap);
  const int len = _tl_str_len(value);
  _TLStr* i = TL_MALLOC(sizeof(_TLStr)+len+1);
  _tl_hold(TL->global, i);
  i->type = TL_STR;
  i->owned = true;
  memcpy(i+1, value, len+1);
  TL->stack[TL->stack_top].ref = NULL;
  TL->stack[TL->stack_top++].object = i;
  return TL->stack_top-1;
}

tl_object_t tl_push_rstr(TLScope* TL, const char *value)
{
  assert(TL->stack_top < TL->stack_cap);
  _TLStr* i = TL_MALLOC(sizeof(_TLStr)+sizeof(char*));
  _tl_hold(TL->global, i);
  i->type = TL_STR;
  i->owned = false;
  memcpy(i+1, &value, sizeof(char*));
  TL->stack[TL->stack_top].ref = NULL;
  TL->stack[TL->stack_top++].object = i;
  return i;
}

int tl_push_float_ex(TLScope* TL, double value)
{
  assert(TL->stack_top < TL->stack_cap);
  _TLFloat* i = TL_MALLOC(sizeof(_TLFloat));
  _tl_hold(TL->global, i);
  i->type = TL_FLOAT;
  i->value = value;
  TL->stack[TL->stack_top].ref = NULL;
  TL->stack[TL->stack_top++].object = i;
  return TL->stack_top-1;
}

int tl_push_bool_ex(TLScope*TL, bool value)
{
  assert(TL->stack_top < TL->stack_cap);
  _TLBool* i = TL_MALLOC(sizeof(_TLBool));
  _tl_hold(TL->global, i);
  i->type = TL_BOOL;
  i->value = value;
  TL->stack[TL->stack_top].ref = NULL;
  TL->stack[TL->stack_top++].object = i;
  return TL->stack_top-1;
}

void tl_fdebug_scope(FILE* f, TLScope* S)
{
  fprintf(
    f, "Stack (%i object long, %i object allocated):\n",
    S->stack_top, S->stack_cap
  );
  fprintf(f, "  Address   Index    Value     [Carried by]\n");
  for (int i = 0; i < S->stack_top; i++)
  {
    fprintf(f, " %p  %08i ", S->stack[i].object, i);
    tl_fdebug_obj(f, S->stack[i].object);
    if (S->stack[i].ref)
    {
      const TLName* name = S->stack[i].ref;
      fprintf(f, "  [%s]", _tl_fmt_name(tl_to_str_pro(S->global->consts[name->global_str_idx])));
    }
    fprintf(f, "\n");
  }
  fprintf(
    f, "Names (total of %i names, %i names allocated):\n",
    S->name_count, S->max_name_count
  );
  fprintf(f, "  Name    Value\n");
  for (int i = 0; i < S->name_count; i++)
  {
    fprintf(f, "  %s = ", _tl_fmt_name(tl_name_to_str(S,i)));
    tl_push_type_name(S, tl_type_of_pro(S->names[i].data));
    printf("(%p)[%s] ", S->names[i].data, tl_to_str(S));
    tl_pop(S);
    tl_fdebug_obj(f, S->names[i].data);
    fprintf(f, "\n");
  }
}

void tl_fdebug_state(FILE *f, TLState *TL)
{
  fprintf(
    f, "Garbage collector (%i objects, %i allocated):\n",
    TL->gc_len, TL->gc_cap
  );
  fprintf(f, "  Address         Ref count  Object\n");
  for (int i = 0; i < TL->gc_len; i++)
  {
    fprintf(f, "  %p  %09i  ", TL->gc_data[i], TL->gc_ref_counts[i]);
    tl_fdebug_obj(f, TL->gc_data[i]);
    fprintf(f, "\n");
  }
  fprintf(
    f, "Constants (%i objects, %i allocated):\n",
    TL->consts_len, TL->consts_cap
  );
  fprintf(f, "  Index     Object\n");
  for (int i = 0; i < TL->consts_len; i++)
  {
    fprintf(f, "  %08i  ", i);
    tl_fdebug_obj(f, TL->consts[i]);
    fprintf(f, "\n");
  }
  fprintf(f, "Global scope:\n");
  tl_fdebug_scope(f, TL->scope);
  fprintf(
    f, "Bytecode (%i instructions, %i allocated):\n",
    TL->bc_len, TL->bc_cap
  );
  fprintf(f, "Index  Instruction (Args)\n");
  for (int i = 0; i < TL->bc_len; i++)
  {
    if (TL->scope->bc_pos-1 == i)
      fprintf(f, ">>");
    else
      fprintf(f, "  ");
    fprintf(f, "%04i   ", i);
    tl_fdebug_instr(f, TL->scope, TL->bc[i]);
  }
}

void tl_custom_info(TLScope* S, TLMSGType msg)
{
  _TLCustom* obj = tl_top(S);
  tl_push_int(S, msg);
  tl_push(S, S->global->consts[obj->info_func]);
  tl_call(S, 2);
}

void tl_fdebug_instr(FILE* f, TLScope* TL, uint32_t code)
{
  switch (tlbc_opcode(code))
  {
    case TLOP_END:
      fprintf(f, "end (depth=%i)\n", tlbc_arg(code));
      break;
    case TLOP_BREAK:
      fprintf(f, "break\n");
      break;
    case TLOP_COND:
      fprintf(f, "cond (depth=%i)\n", tlbc_arg(code));
      break;
    case TLOP_ASSIGN_NAME:
      {
        fprintf(f, "an %i (%s)\n",
                tlbc_arg(code),
                _tl_fmt_name(tl_to_str_pro(TL->global->consts[tlbc_arg(code)]))
        );
        break;
      }
    case TLOP_COPY_NAME:
      {
        fprintf(f, "cn %i (%s)\n",
                tlbc_arg(code),
                _tl_fmt_name(tl_to_str_pro(TL->global->consts[tlbc_arg(code)]))
        );
        break;
      }
    case TLOP_NEW_NAME:
      fprintf(
        f, "nn %i (%s)\n",
        tlbc_arg(code),
        _tl_fmt_name(tl_to_str_pro(TL->global->consts[tlbc_arg(code)]))
      );
      break;
    case TLOP_CONTINUE:
      fprintf(f, "continue\n");
      break;
    case TLOP_COPY_CONST:
      fprintf(f, "cc %i (", tlbc_arg(code));
      tl_fdebug_obj(f, TL->global->consts[tlbc_arg(code)]);
      fprintf(f, ")\n");
      break;
    case TLOP_EXIT:
      fprintf(f, "exit");
      break;
    case TLOP_IF:
      fprintf(f, "if (depth=%i)\n", tlbc_arg(code));
      break;
    case TLOP_PARAM:
      fprintf(f, "param %i\n",
              tlbc_arg(code));
      break;
    case TLOP_POP:
      fprintf(f, "pop %i\n", tlbc_arg(code));
      break;
    case TLOP_EOS:
      fprintf(f, "eos\n");
      break;
    case TLOP_LB:
      fprintf(f, "lb %i\n", tlbc_arg(code));
      break;
    case TLOP_LC:
      fprintf(f, "lc %i\n", tlbc_arg(code));
      break;
    case TLOP_LE:
      fprintf(f, "le\n");
      break;
    case TLOP_WHILE:
      fprintf(f, "while (depth=%i)\n", tlbc_arg(code));
      break;
    case TLOP_ELSE:
      fprintf(f, "else (depth=%i)\n", tlbc_arg(code));
      break;
    case TLOP_FNH:
      fprintf(f, "fnh %i (object=", tlbc_arg(code));
      tl_fdebug_obj(f, TL->global->consts[tlbc_arg(code)]);
      fprintf(f, ")\n");
      break;
    case TLOP_FN:
      fprintf(f, "fn %i\n", tlbc_arg(code));
      break;
    case TLOP_CALL:
      fprintf(f, "call (argc=%i)\n", tlbc_arg(code));
      break;
    case TLOP_RET:
      fprintf(f, "ret (count=%i)\n", tlbc_arg(code));
      break;
  }
}

void tl_fdebug_obj(FILE* f, tl_object_t obj)
{
  switch (tl_type_of_pro(obj))
  {
    case TL_NIL:
      fprintf(f, "nil");
      break;
    case TL_BOOL:
      if (((_TLBool*)obj)->value)
        fprintf(f, "true");
      else
        fprintf(f, "false");
      break;
    case TL_FLOAT:
      fprintf(f, "%lf", ((_TLFloat*)obj)->value);
      break;
    case TL_INT:
      fprintf(f, "%li", ((_TLInt*)obj)->value);
      break;
    case TL_STR:
      {
        const char* s = (char*)obj + sizeof(_TLStr);
        const int len = _tl_str_len(s);
        fprintf(f, "\"");
        for (int i = 0; i < len; i++)
        {
          switch (s[i])
          {
          case '\n': fprintf(f, "\\n"); break;
          case '\r': fprintf(f, "\\r"); break;
          case '\t': fprintf(f, "\\t"); break;
          case '\e': fprintf(f, "\\e"); break;
          case '\a': fprintf(f, "\\a"); break;
          case '\b': fprintf(f, "\\b"); break;
          case '"': fprintf(f, "\\\""); break;
          case '\\': fprintf(f, "\\\\"); break;
          default:
            if (isprint(s[i]))
              fprintf(f, "%c", s[i]);
            else
              fprintf(f, "\\x%02x", (int)s[i]);
            break;
          }
        }
        fprintf(f, "\"");
        break;
      }
    case TL_CFUNC:
      fprintf(
        f, "<C function (%i params) at %p>",
        ((_TLCFunc*)obj)->argc, ((_TLCFunc*)obj)->ptr
      );
      break;
    case TL_FUNC:
      fprintf(
        f, "<ToyLang function (%i params) at #+%i>",
        ((_TLFunc*)obj)->argc, ((_TLFunc*)obj)->bytecode_pt
      );
      break;
    case TL_SCOPE:
      fprintf(
        f, "<Scope stack(%i/%i), names(%i/%i)>",
        tl_to_scope_pro(obj)->stack_top,
        tl_to_scope_pro(obj)->stack_cap,
        tl_to_scope_pro(obj)->name_count,
        tl_to_scope_pro(obj)->max_name_count
      );
      break;
    case TL_CUSTOM:
      fprintf(f,"<custom>");
      break;
  }
}


