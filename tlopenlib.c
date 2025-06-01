#include "tl.h"
#include <string.h>

int _tl_op_call(TLScope *S, int argc)
{
  tl_pop(S);
  return tl_call(S, argc-1);
}

int op_eq(TLScope* TL, int argc)
{
  tl_expect_f(
    argc != 2, 0, "ERROR: Operator '=' needs 2 arguments, not %i\n", argc
  );
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

#define EXPECT_2_ARGS_FAIL(OP) \
  tl_expect_f( \
    argc != 2, 0, \
    "ERROR: Operator '" #OP "' needs 2 arguments, not %i\n", argc \
  );

#define OPCMP_I2H(NAME, OP, TRANSFORM_FLOAT, TRANSFORM_INT, STR_COMPUTE) \
int NAME(TLScope* TL, int argc) \
{ \
  EXPECT_2_ARGS_FAIL(OP) \
  tl_pop(TL); \
  tl_object_t a = tl_get(TL, -2); \
  tl_object_t b = tl_get(TL, -1); \
  const TLType Ta = tl_type_of_pro(a); \
  const TLType Tb = tl_type_of_pro(b); \
  if ( \
    (Ta == TL_FLOAT || Tb == TL_FLOAT) && \
    (Ta == TL_INT || Ta == TL_FLOAT) && \
    (Tb == TL_INT || Tb == TL_FLOAT) \
  ) \
  { \
    double va, vb; \
    if (Ta == TL_FLOAT) \
    { \
      va = tl_to_float_pro(a); \
    } \
    else \
    { \
      va = tl_to_int_pro(a); \
    } \
    if (Tb == TL_FLOAT) \
      vb = tl_to_float_pro(b); \
    else \
      vb = tl_to_int_pro(b); \
    tl_pop(TL); \
    tl_pop(TL); \
    TRANSFORM_FLOAT \
  } \
  else if (Ta == TL_INT && Tb == TL_INT) \
  { \
    TRANSFORM_INT \
  } \
  else if (Ta == TL_STR) \
  STR_COMPUTE \
  else \
  { \
    tl_push_type_name(TL, Ta); \
    const char* Tas = tl_to_str(TL); \
    if (tl_errored()) return 0; \
    tl_push_type_name(TL, Tb); \
    const char* Tbs = tl_to_str(TL); \
    if (tl_errored()) return 0; \
    tl_throw("ERROR: Unknown builtin operation (%s " #OP " %s)\n", \
            Tas, Tbs); \
    return 0; \
  } \
  return 1; \
}



#define CMPI2H(NAME, OP, STR_COMPUTE) \
OPCMP_I2H( \
  NAME, OP, \
  {tl_push_bool(TL, va OP vb);}, \
  {const bool r = tl_to_int_pro(a) OP tl_to_int_pro(b); \
  tl_pop(TL); tl_pop(TL); \
  tl_push_bool(TL, r);}, \
  STR_COMPUTE \
)

#define OPI2H(NAME, OP, STR_COMPUTE) \
OPCMP_I2H( \
  NAME, OP, \
  {tl_push_float(TL, va OP vb);}, \
  {const int64_t r = tl_to_int_pro(a) OP tl_to_int_pro(b); \
  tl_pop(TL); tl_pop(TL); \
  tl_push_int(TL, r);}, \
  STR_COMPUTE \
)

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
if (tl_errored()) return 0;
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

int op_div(TLScope* TL, int argc)
{
  if (argc != 2)
  {
    fprintf(stderr, "ERROR: Operator '/' needs 2 arguments, not %i\n", argc);
    tl_push_nil(TL);
    return 1;
  }
  tl_pop(TL);
  tl_object_t a = tl_get(TL, -2);
  tl_object_t b = tl_get(TL, -1);
  const TLType Ta = tl_type_of_pro(a);
  const TLType Tb = tl_type_of_pro(b);
  if ((Ta == TL_FLOAT || Ta == TL_INT) &&
      (Tb == TL_FLOAT || Tb == TL_INT)
  ) {
    double va, vb;
    if (Ta == TL_FLOAT)
      va = tl_to_float_pro(a);
    else
      va = tl_to_int_pro(a);
    if (Tb == TL_FLOAT)
      vb = tl_to_float_pro(b);
    else
      vb = tl_to_int_pro(b);
    tl_pop(TL);
    tl_pop(TL);
    tl_push_float(TL, va / vb);
  }
  else if (Ta == TL_STR)
    assert(false);
  else
  {
    tl_push_type_name(TL, Ta);
    const char* Tas = tl_to_str(TL);
    tl_push_type_name(TL, Tb);
    const char* Tbs = tl_to_str(TL);
    fprintf(stderr, "ERROR: Unknown builtin operation (%s / %s)\n",
            Tas, Tbs);
    abort();
  }
  return 1;
}

int op_or(TLScope* S, int argc)
{
  if (argc != 2)
  {
    fprintf(stderr, "ERROR: 'or' only accepts two arguments, not %i.", argc);
    abort();
    return 0;
  }
  tl_pop(S);
  if (tl_to_bool_ex(S, -2))
    tl_push(S, tl_get(S, -2));
  else
    tl_push(S, tl_get(S, -1));
  return 1;
}

int op_and(TLScope* S, int argc)
{
  if (argc != 2)
  {
    fprintf(stderr, "ERROR: 'or' only accepts two arguments, not %i.", argc);
    abort();
    return 0;
  }
  tl_pop(S);
  if (tl_to_bool_ex(S, -2))
    tl_push(S, tl_get(S, -1));
  else
    tl_push(S, tl_get(S, -2));
  return 1;
}

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
  char _buf[16];
  const TLData* Da = &S->stack[S->stack_top-2];
  const tl_object_t a = Da->object;
  const TLType Ta = tl_type_of_pro(a);
  const char* attr_name;
  TLScope* look_into;
  _tl_offer_self(a);
  if (Ta == TL_SCOPE)
    look_into = tl_to_scope_pro(a);
  else
  {
    tl_push_type_name(S, Ta);
    const char* Tas = tl_to_str(S);
    TLName* meta = tl_get_name(S, Tas);
    if (tl_type_of_pro(meta->data) != TL_SCOPE)
    {
      fprintf(stdout,
              "ERROR: %s doesn't have a proper meta-scope.\n",
              Tas
      );
      return 0;
    }
    look_into = tl_to_scope_pro(meta->data);
  }
  const TLData* b = tl_top_ex(S);
  if (b->ref)
    // a.b (attribute access)
    attr_name = tl_to_str_pro(S->global->consts[b->ref->global_str_idx]);
  else
  { // a.0 (constant indexing)
    assert(tl_type_of_pro(b->object) == TL_INT);
    const int64_t index =
      tl_to_int_pro(b->object);
    assert(index >= 0);
    _tl_idstr_from_int(_buf, index, sizeof(_buf));
    attr_name = _buf;
  }
  TLName* const child = tl_get_local(look_into, attr_name);
  tl_push(S, child->data);
  tl_top_ex(S)->ref = child;
  return 1;
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

int bool_from(TLScope* TL, int argc)
{
  if (argc == 0)
  {
    tl_push_bool(TL, false);
    return 1;
  }
  tl_pop(TL); // pop funcion
  bool r = tl_to_bool(TL);
  for (int i = 1; i < argc; i++)
  {
    tl_pop(TL);
    r = r && tl_to_bool(TL);
    if (!r) break;
  }
  tl_push_bool(TL, r);
  return 1;
}

int float_from(TLScope* TL, int argc)
{
  if (argc == 0)
  {
    tl_push_float(TL, 0.0);
    return 1;
  }
  tl_pop(TL); // pop funcion
  const tl_object_t t = tl_top(TL);
  const TLType T = tl_type_of(t);
  if (T == TL_STR)
    tl_push_float(TL, atof(tl_to_str(TL)));
  else if (T == TL_INT)
    tl_push_float(TL, tl_to_int(TL));
  else if (T == TL_BOOL)
    tl_push_float(TL, tl_to_bool(TL));
  else if (T == TL_FLOAT)
    tl_push(TL, t);
  else
    assert(false);
  return 1;
}

int int_from(TLScope* TL, int argc)
{
  if (argc == 0)
  {
    tl_push_int(TL, 0);
    return 1;
  }
  tl_pop(TL); // pop funcion
  const tl_object_t t = tl_top(TL);
  const TLType T = tl_type_of(t);
  if (T == TL_STR)
    tl_push_int(TL, atoll(tl_to_str(TL)));
  else if (T == TL_INT)
    tl_push(TL, t);
  else if (T == TL_FLOAT)
    tl_push_int(TL, tl_to_float(TL));
  else if (T == TL_BOOL)
    tl_push_int(TL, tl_to_bool(TL));
  else
    assert(false);
  return 1;
}

int str_from(TLScope* TL, int argc)
{
  static char buf[64];
  if (argc == 0)
  {
    tl_push_rstr(TL, "");
    return 1;
  }
  tl_pop(TL); // pop funcion
  const tl_object_t t = tl_top(TL);
  const TLType T = tl_type_of(t);
  if (T == TL_STR)
    tl_push(TL, t);
  else if (T == TL_INT)
  {
    snprintf(buf, 64, "%lu", tl_to_int(TL));
    tl_push_str(TL, buf);
  }
  else if (T == TL_FLOAT)
  {
    snprintf(buf, 64, "%lf", tl_to_float(TL));
    tl_push_str(TL, buf);
  }
  else if (T == TL_BOOL)
  {
    if (tl_to_bool(TL))
      tl_push_rstr(TL, "true");
    else
      tl_push_rstr(TL, "false");
  }
  else if (T == TL_NIL)
    tl_push_rstr(TL, "nil");
  else
    assert(false);
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

void tl_load_builtins(TLState* TL)
{
#define REG(CSYM, TLSYM, ARGC, PREC) \
  tl_register_func(TL->scope, & CSYM, TLSYM, ARGC, PREC)
  REG(_tl_op_call, "()", 999999, 15);
  REG(op_eq, "=", 2, 1); REG(op_add, "+", 2, 4);
  REG(op_sub, "-", 2, 4); REG(op_mul, "*", 2, 5);
  REG(op_div, "/", 2, 5); REG(op_dot, ".", 2, 11);
  REG(cmp_eq, "==", 2, 3); REG(cmp_neq, "!=", 2, 3);
  REG(cmp_lt, "<", 2, 3);  REG(cmp_gt, ">", 2, 3);
  REG(cmp_le, "<=", 2, 3); REG(cmp_ge, ">=", 2, 3);
  REG(op_or, "or", 2, 2); REG(op_and, "and", 2, 2);
  REG(print, "print", 2, 255);
  REG(_debug_state, "_debug_state", 0, 255);
  REG(_debug_scope, "_debug_scope", 1, 255);
  TLScope* const sc_int = tl_register_scope(TL->scope, "int");
  TLScope* const sc_float = tl_register_scope(TL->scope, "float");
  TLScope* const sc_str = tl_register_scope(TL->scope, "str");
  TLScope* const sc_bool = tl_register_scope(TL->scope, "bool");
  TLScope* const sc_function = tl_register_scope(TL->scope, "function");
  TLScope* const sc_scope = tl_register_scope(TL->scope, "scope");
  tl_register_func(sc_scope, &scope, "new", 0, 255);
  tl_register_func(sc_int, &int_from, "from", 0, 255);
  tl_register_func(sc_float, &float_from, "from", 0, 255);
  tl_register_func(sc_str, &str_from, "from", 0, 255);
  tl_register_func(sc_bool, &bool_from, "from", 0, 255);
}

void tl_load_openlib(TLScope* TL)
{
  TLScope* const math = tl_register_scope(TL, "math");
  
}


