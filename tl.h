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
void _tl_idstr_from_int(char* buf, int64_t val, int n);
const char* _tl_fmt_name(const char* s);
struct TLScope;
typedef void* tl_object_t;
typedef int(*tl_cfunc_ptr_t)(struct TLScope*, int argc);

/*
  WARNING: TL_CUSTOM objects must have their first bytes be:
    TLType type = TL_CUSTOM (or higher);
    <! NO PADDING !>
    int registered_info_function;
    ... other bytes of the custom type ...

  The 'registered_info_function' is the index in the constants of the state of
  a function (or anything that's builtin callable) which takes two parameters:
    1. The object from which ToyLang wishes to recover information from.
    2. The info asked by ToyLang. Can be only one at a time, and only one of:
      - TL_MSG_NAME
        Must return a TL_STR which contains the name of the custom type.
        This is used by the "is" operator and tl_type_to_str().
      - TL_MSG_SIZEOF
        Must return a TL_INT (64 bit signed int) containing the size in bytes
        of the custom type. Only used by tl_size_of().
      - TL_MSG_COPY
        Can return any tl_object. It will then be moved and _tl_hold() to the
        correct location.
      - TL_MSG_REPR
        Must return a TL_STR which contains the code representation of the data
        stored in the type. For hidden type - which I strongly discourage - you
        can return a static string stored in a TL_STR via tl_push_rstr().
      - TL_MSG_DESTROY
        The function must handle all destroying of the type. Including freeing
        the tl_objec_t pointer.
      - TL_SERIALIZE
        The returned object is a "serialized version" of the given object. That
        is, a contiguous block of memory, following the standard for custom
        types, which size is given by a call to the registered_info_function
        with the flag TL_MSG_SIZEOF.
      - TL_DESERIALIZE
        The given object is the "serialized version" as output by a
        TL_SERIALIZE call. The output object is the initial version before the
        TL_SERIALIZE call.
  All five must be declared. In case you don't want to bother writing all of
  them or if any doesn't make sense in your context, you can just return nil,
  in which case a default value will be returned. Note that this can become an
  issue with 'TL_MSG_SIZEOF' if any other custom type depends on the size of
  your object or if your type has to be serialized (the default value being 0).
  The minimum required is TL_MSG_DESTROY, or else memory leaks can occur.
  TL_SERIALIZE and TL_DESERIALIZE will use the object passed as a parameter if
  nil is returned.

*/
typedef enum TLType : uint8_t
{
  TL_NIL, TL_INT, TL_FLOAT, TL_BOOL, TL_STR, TL_FUNC, TL_CFUNC,
  TL_SCOPE, TL_CUSTOM
} TLType;
typedef enum TLMSGType : int64_t
{
  TL_MSG_NAME,
  TL_MSG_SIZEOF,
  TL_MSG_COPY,
  TL_MSG_REPR,
  TL_MSG_DESTROY,
  TL_MSG_SERIALIZE,
  TL_MSG_DESERIALIZE
} TLMSGType;
typedef struct TLName
{
  // char name[MAX_NAME_LENGTH];
  tl_object_t data;
  int global_str_idx;
} TLName;

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
  uint8_t prec;
  int argc;
  int bytecode_pt;
} _TL_VIRT _TLFunc;
typedef struct
{
  TLType type;
  struct TLScope* scope;
} _TL_VIRT _TLScope;
typedef struct
{
  TLType type;
  int info_func;} _TL_VIRT _TLCustom;
typedef struct TLData
{
  tl_object_t object;
  TLName* ref; // associated name, if any
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

#define DEFAULT_STACK_CAP 64
#define DEFAULT_MAX_NAME_COUNT 128
#define TL_INVALID_PATH (TLPathInfo){.index=0, .up=UINT8_MAX}


// === state and scope management ===

TLScope* tl_new_scope_ex(
  TLState* state,
  TLScope* parent,
  int stack_cap,
  int max_name_count,
  int bc_pos // bytecode position
);
TLState* tl_new_state();
void tl_load_builtins(TLState* TL);
void tl_load_openlib(TLScope* TL);
void tl_destroy(TLState* TL);
void tl_destroy_scope(TLScope* TL);
void tl_destroy_object(TLScope* S, tl_object_t obj);
void tl_destroy_custom(TLScope* S, tl_object_t custom);
TLScope* tl_scope_walk_path(TLScope* sc, int up);
char* tl_serialize(TLState* in_TL, size_t* out_size);
void tl_deserialize(TLState* out_TL, char* in_data, size_t size);

// === error management ===
const char* tl_errored();
void tl_throw(const char* error_format, ...);
#define tl_expect_r(EXPECTATION, RETURN_VALUE, fmt) if (!(EXPECTATION)) \
{ tl_throw("ERROR: " fmt "\n"); return RETURN_VALUE; }
#define tl_expect_f(EXPECTATION, RETURN_VALUE, fmt, ...) if (!(EXPECTATION)) \
{ tl_throw("ERROR: " fmt "\n", __VA_ARGS__); return RETURN_VALUE; }

// === gc management ===
void _tl_hold(TLState* TL, tl_object_t obj);
void _tl_drop(TLState* TL, tl_object_t obj);

// === consts management ===
int _tl_consts_push(TLState* TL, tl_object_t obj);
int _tl_consts_push_str(TLState* TL, const char* s);
void tl_register_func(
  TLScope* TL, tl_cfunc_ptr_t f,
  const char* name, int max_arg, int prec
);
TLScope* tl_register_scope(TLScope* TL, const char* name);
int _tl_op_call(TLScope* S, int argc);

// === names ===
const char* tl_name_to_str(TLScope* TL, int idx);
// Creates name if it doesn't exist. Should never returns NULL.
TLName* tl_get_name_ex(TLScope* TL, int global_stack_str_index);
TLName* tl_get_name(TLScope* TL, const char* s);
TLName* tl_get_local(TLScope* TL, const char* s);
// Doesn't create the name. Returns NULL if the name doesn't exist.
TLName* tl_has_name_ex(TLScope* TL, int global_stack_str_index);
// Returns name index (used in bytecode)
int tl_set_local_ex(TLScope* TL, int global_str_name, tl_object_t object);
int tl_set_local(TLScope* TL, const char* name, tl_object_t object);
void tl_copy_name_ex(TLScope* TL, int global_str_name);
void tl_copy_name(TLScope* TL, const char* name);
void _tl_offer_self(tl_object_t obj);
void _tl_take_self(TLScope* S);


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
// Most of these function work with the value put on top of the stack
TLType tl_type_of_pro(tl_object_t obj);
static inline TLType tl_type_of_ex(TLScope* S, int index)
{ return tl_type_of_pro(tl_get(S, index)); }
static inline TLType tl_type_of(TLScope* S)
{ return tl_type_of_pro(tl_top(S)); }
uint64_t tl_size_of_pro(TLScope* S, tl_object_t obj);
static inline uint64_t tl_size_of_ex(TLScope* S, int index)
{ return tl_size_of_pro(S, tl_get(S, index)); }
static inline uint64_t tl_size_of(TLScope* S)
{ return tl_size_of_pro(S, tl_top(S)); }
bool tl_to_bool_pro(tl_object_t obj);
static inline int64_t tl_to_int_pro(tl_object_t obj)
{ assert(tl_type_of_pro(obj) == TL_INT);
  return ((_TLInt*)obj)->value; }
static inline double tl_to_float_pro(tl_object_t obj)
{ assert(tl_type_of_pro(obj) == TL_FLOAT);
  return ((_TLFloat*)obj)->value; }
static inline int tl_precedence_pro(tl_object_t obj)
{
  assert(tl_type_of_pro(obj) == TL_FUNC || tl_type_of_pro(obj) == TL_CFUNC);
  if (tl_type_of_pro(obj) == TL_FUNC)
    return ((_TLFunc*)obj)->prec;
  else
    return ((_TLCFunc*)obj)->prec;
}
static inline int tl_argc_pro(tl_object_t obj)
{
  assert(tl_type_of_pro(obj) == TL_FUNC || tl_type_of_pro(obj) == TL_CFUNC);
  if (tl_type_of_pro(obj) == TL_FUNC)
    return ((_TLFunc*)obj)->argc;
  else
    return ((_TLCFunc*)obj)->argc;
}
static inline char* tl_to_str_pro(tl_object_t obj)
{
  assert(tl_type_of_pro(obj) == TL_STR);
  if (((_TLStr*)obj)->owned)
    return (char*)(((_TLStr*)obj)+1);
  else
    return *(char**)(((_TLStr*)obj)+1);
}
static inline TLScope* tl_to_scope_pro(tl_object_t obj)
{
  assert(tl_type_of_pro(obj) == TL_SCOPE);
  return ((_TLScope*)obj)->scope;
}
#define TO_T_EX(CT, T) \
static inline CT tl_to_##T##_ex(TLScope* S, int index) \
{ return tl_to_##T##_pro(tl_get(S, index)); }
#define TO_T_EZ(CT, T) \
static inline CT tl_to_##T (TLScope* S) \
{ return tl_to_##T##_pro(tl_top(S)); }
#define TO_T(CT, T) \
TO_T_EX(CT, T) \
TO_T_EZ(CT, T)
TO_T(int64_t, int);
TO_T(double, float);
TO_T(char*, str);
// TO_T(bool, bool); // 'bool' is a macro in C :(
static inline bool tl_to_bool_ex(TLScope* S, int index)
{ return tl_to_bool_pro(tl_get(S, index)); }
static inline bool tl_to_bool(TLScope* S)
{ return tl_to_bool_pro(tl_top(S)); }

#undef TO_T_EX
#undef TO_T_EZ
#undef TO_T
void tl_push_type_name(TLScope* S, TLType type);
// push onto the stack the return value, except for TL_MSG_DESTROY
void tl_custom_info(TLScope* S, TLMSGType msg);

//  == setters ==

static inline int tl_push(TLScope* TL, tl_object_t obj)
{
  if (!(TL->stack_top < TL->stack_cap))
  { tl_throw("ERROR: Stack capacity exceeded.\n"); return 0; }
  _tl_hold(TL->global, obj);
  TL->stack[TL->stack_top].object = obj;
  TL->stack[TL->stack_top++].ref = NULL;
  return TL->stack_top-1;
}
void tl_pop(TLScope* TL);
int tl_push_int_ex(TLScope* TL, int64_t value);
int tl_push_str_ex(TLScope *TL, const char *value);
int tl_push_float_ex(TLScope* TL, double value);
int tl_push_bool_ex(TLScope *TL, bool value);
// String can always be referenced as long as the state is alive
tl_object_t tl_push_rstr(TLScope* TL, const char* ref_value);
// Takes ownership of the given string.
// Use tl_push_strcpy to let the state take ownership of the string by itself.
static inline tl_object_t tl_push_nil(TLScope* TL)
{
  assert(TL->stack_top < TL->stack_cap);
  TL->stack[TL->stack_top].ref = NULL;
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
  TLOP_SELF, // calls _tl_take_self
  TLOP_NEW_NAME, // nn <global_stack_str_name_index>
  TLOP_ASSIGN_NAME, // an <global_stack_str_name_index>
  TLOP_COPY_NAME, // cg <global_stack_str_name_index>
  TLOP_COPY_CONST, // cc <stack_index>
  TLOP_COPY_SMALL_INT, // csi <value>
  TLOP_COPY_BOOL, // cbool <value>
  TLOP_COPY_NIL, // cnil
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
  TLOP_LB, // lb <length_until_next_comma> // List Begin
  TLOP_LC, // lc <length> // List Comma
  TLOP_LE, // le // List End
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
  int length,
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
typedef struct TokenizerOut TokenizerOut;
void tl_expr_to_rpn(
  TLState* G,
  TokenizerOut* toks,
  int start, int len);

#define TOKEN_STR_MAX MAX_NAME_LENGTH
#define TOKEN_STACK_MAX 1024
#define DEFAULT_BYTECODE_ALLOC 1024

// These hashes are just the 8 bytes that make up the string
// Will be improved in the future
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
#define HASH_NIL 0x6e696cUL
#define HASH_TRUE 0x74727565UL
#define HASH_FALSE 0x66616c7365UL
typedef enum TokenType : uint8_t
{
  // TOK     , <arg>
  TOK_NOTHING,
  TOK_NAME, // <name_index>
  TOK_OP, // <name_index> 
  TOK_INT, // <stack_index>
  TOK_SMALL_INT, // <value>
  TOK_NIL, TOK_TRUE, TOK_FALSE,
  TOK_FLOAT, // <stack_index>
  TOK_STR, // <stack_index>
  TOK_COMMA,
  TOK_PARENTH, // <length until ')' (in tokens)>
  TOK_BRACKET, // <length until ']' (in tokens)>
  TOK_CALL, // special token to indicate function call
  TOK_INDEX, // special token to indicate indexing
  TOK_LC, // 'lc' instruction
  
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
  int max_prec;
} TokenizerOut;
static const TokenizerOut TOKENIZER_EOF = (TokenizerOut){{}, -1, -1};
TokenizerOut tl_tokenize_string(TLState *TL, TokenizerState* TS, char *code);
void tl_parse_to_bytecode(TLState* TL, char* code);
void tl_debug_token_array(TLState* TL, Token* a, int len, int current);

#endif

