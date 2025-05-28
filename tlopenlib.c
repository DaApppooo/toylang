#include "tl.h"

int _tl_op_call(TLScope *S, int argc)
{
  tl_pop(S);
  tl_call(S, argc-1);
  // official returns 0 values because the return values are
  // already set up in the initial stack, no need to move anything.
  return 0;
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
  tl_object_t src = tl_top(TL);
  _tl_hold(TL->global, src);
  tl_pop(TL); // pop second argument
  TLData* dst = tl_top_ex(TL);
  TLName* n = tl_name_walk_path(TL, dst->carrier);
  _tl_drop(TL->global, n->data);
  n->data = src;
  return 1;
}

int op_add(TLScope* TL, int argc)
{
  if (argc != 2)
  {
    fprintf(stderr, "ERROR: Operator '+' needs 2 arguments, not %i\n", argc);
    tl_push_nil(TL);
    return 1;
  }
  tl_pop(TL);
  tl_object_t a = tl_top(TL);
  tl_pop(TL);
  tl_object_t b = tl_top(TL);
  const TLType Ta = tl_type_of(a);
  const TLType Tb = tl_type_of(b);
  if (Ta == TL_FLOAT || Tb == TL_FLOAT)
  {
    assert(Ta == TL_INT || Ta == TL_FLOAT);
    assert(Tb == TL_INT || Tb == TL_FLOAT);
    double va, vb;
    if (Ta == TL_FLOAT)
    {
      va = tl_to_float(a);
    }
    else
    {
      assert(Ta == TL_INT);
      va = tl_to_int(a);
    }
    if (Tb == TL_FLOAT)
      vb = tl_to_float(b);
    else
      vb = tl_to_int(b);
    tl_push_float(TL, va + vb);
  }
  else if (Ta == TL_INT)
  {
    if (Tb != TL_INT)
    {
      fprintf(stderr, "ERROR: Cannot add an int with a %s value",
              tl_type_to_str(Tb));
      tl_push_nil(TL);
      return 1;
    }
    tl_push_int(TL, tl_to_int(a) + tl_to_int(b));
  }
  else
  {
    fprintf(stderr, "TODO (%s + %s)\n",
            tl_type_to_str(Ta), tl_type_to_str(Tb));
    assert(false);
  }
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
  tl_register_func(TL, &print, "print", 2, 255);
  tl_register_func(TL, &_debug_state, "_debug_state", 0, 255);
}


