#include "tl.h"
#include <assert.h>
#include <ctype.h>
#include <ctype.h>
#include <ctype.h>
#include <stdarg.h>
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
  static char buf[32];
  if (*s == '@')
  {
    const uint64_t i = *(uint64_t*)s;
    snprintf(buf, 32, "@%lu", (i >> 8));
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

#define TL_ERROR_MAX 1024
char _tl_error[TL_ERROR_MAX] = {0};
const char* tl_errored()
{
  if (_tl_error[0])
    return _tl_error;
  else
    return NULL;
}
void tl_throw(const char* error_format, ...)
{
  va_list args;
  va_start(args, error_format);
  vsnprintf(_tl_error, TL_ERROR_MAX, error_format, args);
  va_end(args);
}

tl_object_t _offered_self = NULL;
void _tl_offer_self(tl_object_t obj)
{ _offered_self = obj; }
void _tl_take_self(TLScope* S)
{
  tl_set_local(S, "self", _offered_self);
  _offered_self = NULL;
}

// Call function on top of the stack
// Returns the amount of returned values
int tl_call(TLScope* S, int argc)
{
  if (tl_errored()) return 0;
  const int old_stack_top = S->stack_top;
  const TLType func_type = tl_type_of(S);
  const int prearg_stack_top = old_stack_top - argc - 1;
  int retc = 0;
  if (func_type == TL_NIL)
  {
    tl_throw("ERROR: Calling a nil value.\n");
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
    if (tl_errored()) return 0;
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
    if (tl_errored()) return 0;
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
    if (tl_errored()) return 0;
    const char* type = tl_to_str(S);
    if (tl_errored()) return 0;
    tl_throw("ERROR: %s is not callable by default.",type);
    tl_pop(S);
    return 0;
  }
  if (S->stack_top < 0)
    S->stack_top = 0;
  if (tl_errored()) return 0;
  return retc;
}

void tl_register_func(
  TLScope* TL, tl_cfunc_ptr_t f,
  const char* name, int max_arg, int prec
) {
  _TLCFunc* tl_f = TL_MALLOC(sizeof(_TLCFunc));
  tl_f->argc = max_arg;
  tl_f->prec = prec;
  tl_f->ptr = f;
  tl_f->type = TL_CFUNC;
  tl_set_local(TL, name, tl_f);
}

TLScope* tl_register_scope(TLScope* TL, const char* name)
{
  _TLScope* virt = TL_MALLOC(sizeof(_TLScope));
  virt->type = TL_SCOPE;
  virt->scope = tl_new_scope_ex(
    TL->global, TL, TL->stack_cap, TL->max_name_count, TL->bc_pos
  );
  tl_set_local(TL, name, virt);
  return virt->scope;
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
  const int i = tl_set_local_ex(org, global_stack_str_index, NULL);
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
  const int i = tl_set_local(org, name, NULL);
  return org->names + i;
}
TLName* tl_get_local(TLScope* S, const char* s)
{
  for (int i = 0; i < S->name_count; i++)
  {
    if (_tl_str_eq(tl_name_to_str(S, i), s))
      return S->names + i;
  }
  const int i = tl_set_local(S, s, NULL);
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
    DEFAULT_MAX_NAME_COUNT, 0
  );
  return TL;
}

TLScope* tl_new_scope_ex(
  TLState* state,
  TLScope* parent,
  int mem_size, int max_name_count,
  int bc_pos
) {
  TLScope* self = TL_MALLOC(sizeof(TLScope));
  self->bc_pos = bc_pos;
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
  tl_expect_r(f != NULL,, "ERROR: Invalid input file.");
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

bool tl_to_bool_pro(tl_object_t obj)
{
  const int type = tl_type_of_pro(obj);
  switch (type)
  {
    case TL_INT: return tl_to_int_pro(obj) != 0;
    case TL_FLOAT: return tl_to_float_pro(obj) != 0;
    case TL_CFUNC:
    case TL_FUNC:
    case TL_CUSTOM:
      return true;
    case TL_NIL:
      return false;
    case TL_BOOL:
      return ((_TLBool*)obj)->value;
    case TL_STR:
      {
      const char* s = tl_to_str_pro(obj);
      return !(s == NULL || *s == 0);
      }
    case TL_SCOPE:
      {
      const TLScope* sc = tl_to_scope_pro(obj);
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
  for (int i = 0; i < TL->consts_len; i++)
  {
    if (tl_type_of_pro(TL->consts[i]))
      if (_tl_str_eq(tl_to_str_pro(TL->consts[i]), s))
        return i;
  }
  const int l = _tl_str_len(s);
  _TLStr* i = TL_MALLOC(sizeof(_TLStr) + l+1);
  i->type = TL_STR;
  i->owned = true;
  memcpy(i+1, s, l+1);
  const int r = _tl_consts_push(TL, i);
  return r;
}

int tl_set_local_ex(TLScope* S, int name_str_global, tl_object_t object)
{
  tl_expect_r(S->name_count < S->max_name_count, 0,
            "Name count exceeded capacity.");
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
  _tl_hold(S->global, object);
  return S->name_count-1;
}

int tl_set_local(TLScope* S, const char* name, tl_object_t object)
{
  tl_expect_f(
    S->name_count < S->max_name_count, 0,
    "Name count exceeded capacity with name '%s'", name
  );
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
  _tl_hold(S->global, object);
  return S->name_count-1;
}


int tl_push_int_ex(TLScope* S, int64_t value)
{
  tl_expect_r(S->stack_top < S->stack_cap, 0, "Stack capacity exceeded.");
  _TLInt* i = TL_MALLOC(sizeof(_TLInt));
  _tl_hold(S->global, i);
  i->type = TL_INT;
  i->value = value;
  S->stack[S->stack_top].ref = NULL;
  S->stack[S->stack_top++].object = i;
  return S->stack_top-1;
}

int tl_push_str_ex(TLScope* S, const char *value)
{
  tl_expect_r(S->stack_top < S->stack_cap, 0, "Stack capacity exceeded.");
  const int len = _tl_str_len(value);
  _TLStr* i = TL_MALLOC(sizeof(_TLStr)+len+1);
  _tl_hold(S->global, i);
  i->type = TL_STR;
  i->owned = true;
  memcpy(i+1, value, len+1);
  S->stack[S->stack_top].ref = NULL;
  S->stack[S->stack_top++].object = i;
  return S->stack_top-1;
}

tl_object_t tl_push_rstr(TLScope* S, const char *value)
{
  tl_expect_r(S->stack_top < S->stack_cap, 0, "Stack capacity exceeded.");
  _TLStr* i = TL_MALLOC(sizeof(_TLStr)+sizeof(char*));
  _tl_hold(S->global, i);
  i->type = TL_STR;
  i->owned = false;
  memcpy(i+1, &value, sizeof(char*));
  S->stack[S->stack_top].ref = NULL;
  S->stack[S->stack_top++].object = i;
  return i;
}

int tl_push_float_ex(TLScope* S, double value)
{
  tl_expect_r(S->stack_top < S->stack_cap, 0, "Stack capacity exceeded.");
  _TLFloat* i = TL_MALLOC(sizeof(_TLFloat));
  _tl_hold(S->global, i);
  i->type = TL_FLOAT;
  i->value = value;
  S->stack[S->stack_top].ref = NULL;
  S->stack[S->stack_top++].object = i;
  return S->stack_top-1;
}

int tl_push_bool_ex(TLScope* S, bool value)
{
  tl_expect_r(S->stack_top < S->stack_cap, 0, "Stack capacity exceeded.");
  _TLBool* i = TL_MALLOC(sizeof(_TLBool));
  _tl_hold(S->global, i);
  i->type = TL_BOOL;
  i->value = value;
  S->stack[S->stack_top].ref = NULL;
  S->stack[S->stack_top++].object = i;
  return S->stack_top-1;
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
  if (tl_errored()) return;
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
    case TLOP_SELF:
      fprintf(f, "param self %i\n", tlbc_arg(code));
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
    case TLOP_COPY_SMALL_INT:
      fprintf(f, "csi %i\n", tlbc_arg(code));
      break;
    case TLOP_COPY_BOOL:
      fprintf(f, "cbool %i\n", tlbc_arg(code));
      break;
    case TLOP_COPY_NIL:
      fprintf(f, "cnil\n");
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
        const char* s = tl_to_str_pro(obj);
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
              fprintf(f, "\\x%02hhx", s[i]);
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


