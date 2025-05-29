#include "tl.h"
#include <string.h>

int _tl_op_call(TLScope *S, int argc)
{
  tl_pop(S);
  return tl_call(S, argc-1);;
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
  TLName* n = tl_name_walk_path(TL, dst->carrier);
  _tl_drop(TL->global, n->data);
  n->data = src;
  _tl_hold(TL->global, n->data);
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
  const TLType Ta = tl_type_of(a); \
  const TLType Tb = tl_type_of(b); \
  if (Ta == TL_FLOAT || Tb == TL_FLOAT) \
  { \
    assert(Ta == TL_INT || Ta == TL_FLOAT); \
    assert(Tb == TL_INT || Tb == TL_FLOAT); \
    double va, vb; \
    if (Ta == TL_FLOAT) \
    { \
      va = tl_to_float(a); \
    } \
    else \
    { \
      assert(Ta == TL_INT); \
      va = tl_to_int(a); \
    } \
    if (Tb == TL_FLOAT) \
      vb = tl_to_float(b); \
    else \
      vb = tl_to_int(b); \
    tl_pop(TL); \
    tl_pop(TL); \
    tl_push_float(TL, va OP vb); \
  } \
  else if (Ta == TL_INT) \
  { \
    if (Tb != TL_INT) \
    { \
      fprintf(stderr, "ERROR: Unknown builtin operation " #OP " between an int and a %s\n", \
              tl_type_to_str(Tb)); \
      tl_push_nil(TL); \
      return 1; \
    } \
    int64_t r = tl_to_int(a) OP tl_to_int(b); \
    tl_pop(TL); \
    tl_pop(TL); \
    tl_push_int(TL, r); \
  } \
  else if (Ta == TL_STR) \
  STR_COMPUTE \
  else \
  { \
    fprintf(stderr, "ERROR: Unknown builtin operation (%s " #OP " %s)\n", \
            tl_type_to_str(Ta), tl_type_to_str(Tb)); \
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
  const TLType Ta = tl_type_of(a); \
  const TLType Tb = tl_type_of(b); \
  if (Ta == TL_FLOAT || Tb == TL_FLOAT) \
  { \
    assert(Ta == TL_INT || Ta == TL_FLOAT); \
    assert(Tb == TL_INT || Tb == TL_FLOAT); \
    double va, vb; \
    if (Ta == TL_FLOAT) \
    { \
      va = tl_to_float(a); \
    } \
    else \
    { \
      assert(Ta == TL_INT); \
      va = tl_to_int(a); \
    } \
    if (Tb == TL_FLOAT) \
      vb = tl_to_float(b); \
    else \
      vb = tl_to_int(b); \
    tl_pop(TL); \
    tl_pop(TL); \
    tl_push_bool(TL, va OP vb); \
  } \
  else if (Ta == TL_INT) \
  { \
    if (Tb != TL_INT) \
    { \
      fprintf(stderr, "ERROR: Unknown builtin operation " #OP " between an int and a %s\n", \
              tl_type_to_str(Tb)); \
      tl_push_nil(TL); \
      return 1; \
    } \
    bool r = tl_to_int(a) OP tl_to_int(b); \
    tl_pop(TL); \
    tl_pop(TL); \
    tl_push_bool(TL, r); \
  } \
  else if (Ta == TL_STR) \
  STR_COMPUTE \
  else \
  { \
    fprintf(stderr, "ERROR: Unknown builtin operation (%s " #OP " %s)\n", \
            tl_type_to_str(Ta), tl_type_to_str(Tb)); \
    assert(false); \
  } \
  return 1; \
}

OPI2H(op_add, +, {
if (Tb != TL_STR)
{
  fprintf(stderr, "ERROR: Builtins do not allow sum of str and %s.\n",
    tl_type_to_str(Tb));
  assert(false);
}
const char* sa = tl_to_str(a);
const char* sb = tl_to_str(b);
const int la = _tl_str_len(sa);
const int lb = _tl_str_len(sb);
_TLStr* out = TL_MALLOC(sizeof(_TLStr) + sizeof(char)*(la+lb+1));
out->owned = true;
out->type = TL_STR;
char* p = (char*)(out+1);
memcpy(p, sa, la);
memcpy(p+la, sb, lb+1);
_tl_hold(TL->global, out);
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


int print(TLScope* TL, int argc)
{
  if (argc == 0)
  {
    puts("");
    return 0;
  }
  tl_pop(TL); // pop funcion
  tl_object_t o = tl_get(TL, -argc);
  if (tl_type_of(o) == TL_STR)
    printf("%s", tl_to_str(o));
  else
    tl_fdebug_obj(stdout, o);
  for (int i = 1; i < argc; i++)
  {
    o = tl_get(TL, i-argc);
    printf("\t");
    if (tl_type_of(o) == TL_STR)
      printf("%s", tl_to_str(o));
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


void tl_load_openlib(TLState* TL)
{
  tl_register_func(TL, &_tl_op_call, "()", 999999, 15);
  tl_register_func(TL, &op_eq, "=", 2, 1);
  tl_register_func(TL, &op_add, "+", 2, 4);
  tl_register_func(TL, &op_sub, "-", 2, 4);
  tl_register_func(TL, &op_mul, "*", 2, 5);
  tl_register_func(TL, &print, "print", 2, 255);
  tl_register_func(TL, &_debug_state, "_debug_state", 0, 255);
}


