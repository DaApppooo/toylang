#ifndef H_TOYLANG
#define H_TOYLANG
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifndef TL_MALLOC
#define TL_MALLOC malloc
#endif
#ifndef TL_FREE
#define TL_FREE free
#endif
#ifndef TL_REALLOC
#define TL_REALLOC realloc
#endif

/*
 This toy language is an exercise to force me to be lazy and avoid any data
 oriented design. This is horribly painful and must be ugly to see from the
 outside. I am once again sorry for the war crimes I'm commiting in this file
 (/j). This is a toy scripting language with whatever I feel is missing from
 lua.
*/

#ifndef MAX_NAME_LENGTH
#define MAX_NAME_LENGTH (256-8)
#endif


char* _tl_str_clone(const char* const s);
int _tl_str_len(const char* const s);
bool _tl_str_eq(const char* s1, const char* s2);
struct TLScope;
typedef void* tl_object_t;
typedef int(*tl_cfunc_ptr_t)(struct TLScope*, int argc);

typedef enum TLType : uint8_t
{
  TL_NIL, TL_INT, TL_FLOAT, TL_BOOL, TL_STR, TL_LIST, TL_FUNC, TL_CFUNC,
  TL_CUSTOM
} TLType;
typedef struct TLName
{
  // char name[MAX_NAME_LENGTH];
  tl_object_t data;
  int global_str_idx;
} TLName;
typedef struct
{
  uint16_t index;
  uint8_t up; // go up 'up' times in the TLState tree
} TLPathInfo;

// These are containers used to ease the interpreter's interaction with objects
#define _TL_VIRT __attribute__((packed, aligned(1)))
struct TLState;
// TL_INT
typedef struct
{
  TLType type;
  int64_t value;
} _TL_VIRT _TLInt;
// TL_FLOAT
typedef struct
{
  TLType type;
  double value;
} _TL_VIRT _TLFloat;
// TL_STR
typedef struct
{
  TLType type;
  uint8_t owned;
  // NOTE: if not owned: char* ref;
  // otherwise: string content put here
} _TL_VIRT _TLStr;
// TL_BOOL
typedef struct
{
  TLType type;
  uint8_t value;
} _TL_VIRT _TLBool;
// TL_LIST
typedef struct
{
  TLType type;
  uint64_t len;
  uint64_t cap;
  // then followed by 8*len bytes of pointers to data 
} _TL_VIRT _TLList;
// TL_CFUNC
typedef struct
{
  TLType type;
  uint8_t prec;
  int argc; // if negative, then arguments can be of type TL__NAME_PTR
  tl_cfunc_ptr_t ptr;
} _TL_VIRT _TLCFunc;
// TL_FUNC
typedef struct
{
  TLType type;
  struct TLState* child;
  uint8_t prec;
  int argc;
  int bytecode_pt;
} _TL_VIRT _TLFunc;
typedef struct TLData
{
  tl_object_t object;
  TLPathInfo carrier; // associated name, if any
} TLData;
struct TLState;
typedef struct TLScope
{
  struct TLState* global;
  struct TLScope* parent;
  TLData* stack;
  TLName* names;
  int bc_pos;
  int stack_top;
  int stack_cap;
  int max_name_count;
  int name_count;
} TLScope;
typedef struct TLState
{
  TLScope* scope;
  tl_object_t* consts;
  tl_object_t* gc_data;
  uint16_t* gc_ref_counts;
  uint32_t* bc;
  int consts_len;
  int consts_cap;
  int gc_len;
  int gc_cap;
  int bc_len;
  int bc_cap;
} TLState;

#define DEFAULT_STACK_CAP 25
#define DEFAULT_MAX_NAME_COUNT 128
#define TL_INVALID_PATH (TLPathInfo){.index=0, .up=UINT8_MAX}


// === state and scope management ===

static inline bool tl_is_valid_path(TLPathInfo pi)
{
  return pi.up < UINT8_MAX;
}
TLScope* tl_new_scope_ex(
  TLState* state,
  TLScope* parent,
  int stack_cap,
  int max_name_count,
  int child_bcpos
);
TLState* tl_new_state();
void tl_load_openlib(TLState* TL);
void tl_destroy(TLState* TL);
void tl_destroy_scope(TLScope* TL);
TLScope* tl_scope_walk_path(TLScope* sc, int up);
TLType tl_type_of(tl_object_t obj);
uint64_t tl_size_of(tl_object_t obj);

// === gc management ===
void _tl_hold(TLState* TL, tl_object_t obj);
void _tl_drop(TLState* TL, tl_object_t obj);

// === consts management ===
int _tl_consts_push(TLState* TL, tl_object_t obj);
int _tl_consts_push_str(TLState* TL, const char* s);
void tl_register_func(
  TLState* TL, tl_cfunc_ptr_t f,
  const char* name, int max_arg, int prec
);
int _tl_op_call(TLScope* S, int argc);

// === names ===
TLPathInfo _tl_piname(TLScope* TL, const char* name);
const char* tl_name_to_str(TLScope* TL, int idx);
TLName* tl_name_walk_path(TLScope* TL, TLPathInfo path);
// Creates name if it doesn't exist. Should never returns NULL.
TLName* tl_get_name_ex(TLScope* TL, int global_stack_str_index);
// Creates name if it doesn't exist. Should never returns NULL.
TLName* tl_get_name(TLScope* TL, const char* s);
// Doesn't create the name. Returns NULL if the name doesn't exist.
TLName* tl_has_name_ex(TLScope* TL, int global_stack_str_index);
// Returns name index (used in bytecode)
int tl_set_name_ex(TLScope* TL, int global_str_name, tl_object_t object);
int tl_set_name(TLScope* TL, const char* name, tl_object_t object);
void tl_copy_name_ex(TLScope* TL, int global_str_name);
void tl_copy_name(TLScope* TL, const char* name);
static inline int tl_push(TLScope* TL, tl_object_t obj)
{
  assert(TL->stack_top < TL->stack_cap);
  TL->stack[TL->stack_top].object = obj;
  TL->stack[TL->stack_top++].carrier = TL_INVALID_PATH;
  return TL->stack_top-1;
}


// === stack values ===
//  == getters ==

// Call function on top of the stack
// returns C's exit status (EXIT_FAILURE or EXIT_SUCCESS)
int tl_call(TLScope* S, int argc);
static inline tl_object_t tl_get(TLScope* S, int index)
{
  if (index < 0)
    index = S->stack_top + index;
  if (index < 0 || index >= S->stack_top)
    return NULL;
  return S->stack[index].object;
}
static inline TLData* tl_top_ex(TLScope* S)
{ return &(S->stack[S->stack_top-1]); }
static inline tl_object_t tl_top(TLScope* S)
{ return tl_top_ex(S)->object; }
bool tl_top_to_bool(TLScope* S);
static inline int64_t tl_to_int(tl_object_t obj)
{ assert(tl_type_of(obj) == TL_INT); return ((_TLInt*)obj)->value; }
static inline double tl_to_float(tl_object_t obj)
{ assert(tl_type_of(obj) == TL_FLOAT); return ((_TLFloat*)obj)->value; }
static inline int tl_precedence(tl_object_t object)
{
  assert(tl_type_of(object) == TL_FUNC || tl_type_of(object) == TL_CFUNC);
  if (tl_type_of(object) == TL_FUNC)
    return ((_TLFunc*)object)->prec;
  else
    return ((_TLCFunc*)object)->prec;
}
static inline int tl_argc(tl_object_t object)
{
  assert(tl_type_of(object) == TL_FUNC || tl_type_of(object) == TL_CFUNC);
  if (tl_type_of(object) == TL_FUNC)
    return ((_TLFunc*)object)->argc;
  else
    return ((_TLCFunc*)object)->argc;
}
static inline const char* tl_to_str(tl_object_t obj)
{
  assert(tl_type_of(obj) == TL_STR);
  // if (((_TLStr*)obj)->owned)
  return (char*)(((_TLStr*)obj)+1);
  // else
  //   return *(char**)(((_TLStr*)obj)+1);
}
static inline bool tl_to_bool(tl_object_t obj)
{
  assert(tl_type_of(obj) == TL_BOOL);
  return ((_TLBool*)obj)->value;
}

static inline const char* tl_type_to_str(TLType type)
{
  const char* TABLE[TL_CUSTOM+1] = {
    "nil", "int", "float", "bool", "str", "list", "function", "cfunction",
    "custom"
  };
  return TABLE[type];
}

//  == setters ==

void tl_pop(TLScope* TL);
int tl_push_int_ex(TLScope* TL, int64_t value);
int tl_push_str_ex(TLScope *TL, const char *value);
int tl_push_float_ex(TLScope* TL, double value);
int tl_push_bool_ex(TLScope *TL, bool value);
// String can always be referenced as long as the state is alive
tl_object_t tl_push_rstr(TLState* TL, char* ref_value);
// Takes ownership of the given string.
// Use tl_push_strcpy to let the state take ownership of the string by itself.
static inline tl_object_t tl_push_nil(TLScope* TL)
{
  assert(TL->stack_top < TL->stack_cap);
  TL->stack[TL->stack_top].carrier = TL_INVALID_PATH;
  TL->stack[TL->stack_top++].object = NULL;
  return NULL;
}
static inline tl_object_t tl_push_str(TLScope* TL, const char* ref_value)
{ return TL->stack[tl_push_str_ex(TL, ref_value)].object; }
static inline tl_object_t tl_push_float(TLScope* TL, double value)
{ return TL->stack[tl_push_float_ex(TL, value)].object; }
static inline tl_object_t tl_push_int(TLScope* TL, int64_t value)
{ return TL->stack[tl_push_int_ex(TL, value)].object; }
static inline tl_object_t tl_push_bool(TLScope* TL, bool value)
{ return TL->stack[tl_push_bool_ex(TL, value)].object; }
tl_object_t tl_begin_list(TLScope* TL);
tl_object_t tl_end_list(TLScope* TL);


// === debug stuff ===

void tl_fdebug_state(FILE* f, TLState* TL);
void tl_fdebug_instr(FILE* f, TLScope* TL, uint32_t code);
void tl_fdebug_obj(FILE* f, tl_object_t obj);
void tl_fdebug_scope(FILE* f, TLScope* S);


// === bytecode ===

// These are the high byte of a u32 which also encodes a 3 byte parameter 
typedef enum : uint8_t
{
  TLOP_EXIT, // exit
  // function header
  // must be followed by 'fn' (and 'fn' must be preceeded by )
  // fnh <global_stack_function_index>
  TLOP_FNH, 
  TLOP_FN, // fn <global_stack_str_name_index> // nn + jump to end
  TLOP_NEW_NAME, // nn <global_stack_str_name_index>
  TLOP_ASSIGN_NAME, // an <global_stack_str_name_index>
  TLOP_COPY_NAME, // cg <global_stack_str_name_index>
  TLOP_COPY_CONST, // cc <stack_index>
  TLOP_POP, // pop <count>
  // Calls function at the top of the stack,
  // with parameters pushed in order.
  TLOP_CALL, // call <passed_param_count>
  // Assign in order names of parameters with their effective address
  // (when called).
  TLOP_PARAM, // param <name_index>
  // Declares the begining of a conditional computation (either for if/elifs or
  // while). Values understood as false are: false, nil and 0 (float or int)
  TLOP_COND, // cond <depth>
  // If value on top of the stack is true, just continue execution as usual,
  // except when TLOP_COND with same depth as previous cond's depth is
  // encountered, jumps forward 4bytes until TLOP_END with same depth is
  // encountered.
  // Otherwise jumps forward 4bytes until following operation (with same
  // parameter) is found:
  // - TLOP_COND
  // - TLOP_ELSE
  // - TLOP_END
  // 'depth' should be the same as preceding 'cond' instruction.
  TLOP_IF, // if <depth>
  // NOTE: ELIF = COND+IF of same depth
  TLOP_ELSE, // else <depth>
  // Same as IF, except when TLOP_END (with same depth) is reached, jumps back
  // to previous COND.
  // If condition is false, jumps to next:
  // - TLOP_ELSE
  // - TLOP_END
  // While statement can also later be used to support for loops with iterators
  TLOP_WHILE, // while <depth>
  TLOP_BREAK, // break
  TLOP_CONTINUE, // continue
  TLOP_RET, // ret <count>
  TLOP_END, // end <depth>
  TLOP_EOS, // eos // End Of Statement,
                   // avoid overflowing the stack with unused returned value
                   // put between statements
} TLOpCode;
#define CODE(OPCODE, OPERAND) ((OPCODE<<24)|(OPERAND))
static inline TLOpCode tlbc_opcode(uint64_t unit)
{ return (TLOpCode)((unit >> 24) & 0xff); }
static inline uint32_t tlbc_arg(uint64_t unit)
{ return unit & 0x00ffffff; }
void tl_run_bytecode(TLState* TL);
int tl_run_bytecode_ex(
  TLScope* TL,
  tl_object_t scope,
  int depth,
  int param_count
);


// === parsing and compilation ===

void tl_compile_string(TLState* TL, char* ref_code);
static inline void tl_do_string(TLState* TL, char* ref_code)
{
  tl_compile_string(TL, ref_code);
  tl_run_bytecode(TL);
}
void tl_do_file(TLState* TL, FILE* ref_f);
void tl_do_path(TLState* TL, const char* ref_path);

#define TOKEN_STR_MAX MAX_NAME_LENGTH
#define TOKEN_STACK_MAX 1024
#define DEFAULT_BYTECODE_ALLOC 1024
#define HASH_FN 0x666eUL
#define HASH_IF 0x6966UL
#define HASH_ELIF 0x656c6966UL
#define HASH_ELSE 0x656c7365UL
#define HASH_WHILE 0x7768696c65UL
#define HASH_THEN 0x7468656eUL
#define HASH_DO 0x646fUL
#define HASH_BREAK 0x627265616bUL
#define HASH_CONTINUE 0x636f6e74696e7565UL
#define HASH_RETURN 0x72657475726eUL
#define HASH_END 0x656e64UL
typedef enum TokenType : uint8_t
{
  // TOK     , <arg>
  TOK_NOTHING,
  TOK_NAME, // <name_index>
  TOK_OP, // <name_index> 
  TOK_INT, // <stack_index>
  // once a dot has been detected, TOK_INT becomes TOK_FLOAT
  TOK_FLOAT, // <stack_index>
  TOK_STR, // <stack_index>
  TOK_COMMA,
  TOK_PARENTH, // <length until ')' (in tokens)>
  TOK_CALL, // special token to indicate function call
  
  TOK_FN, // <name_index>
  TOK_IF, // becomes TLOP_COND
  TOK_ELIF, // becomes TLOP_COND
  TOK_ELSE,
  TOK_WHILE, // becomes TLOP_COND
  TOK_THEN, // becomes TLOP_IF (yeah, make it make sense)
  TOK_DO, // becomes TLOP_WHILE
  TOK_BREAK,
  TOK_CONTINUE,
  TOK_RETURN,
  TOK_END
} TokenType;
typedef struct Token
{
  TokenType type : 8;
  uint32_t arg : 24;
  // 0 => token hasn't been moved into a rpn layout
  // otherwise(n) => following n tokens have been moved into a rpn layout
  //                 and must be moved together
  uint16_t rpned;
  uint16_t op_argc; // only for operators and functions
} Token;
static const uint32_t UNSET_PARENTH = 0xffffff;
typedef struct TokenizerState
{
  int i, len;
} TokenizerState;
typedef struct TokenizerOut
{
  Token toks[TOKEN_STACK_MAX];
  int toksi;
  int max_depth;
  int max_prec;
} TokenizerOut;
static const TokenizerOut TOKENIZER_EOF = (TokenizerOut){{}, -1, -1, -1};
TokenizerOut tl_tokenize_string(TLState *TL, TokenizerState* TS, char *code);
void tl_parse_to_bytecode(TLState* TL, char* code);
void tl_debug_token_array(TLState* TL, Token* a, int len, int current);

#endif

