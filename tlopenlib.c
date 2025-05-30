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
      _tl_strn_from_int(_buf+1, index, sizeof(_buf)-1);
      _buf[0] = '@';
      TLName* const child = tl_get_local(scope, _buf);
      tl_push(scope, child->data);
      tl_top_ex(scope)->ref = child;
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
}


