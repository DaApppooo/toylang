#include "tl.h"

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
  if (Ta == TL_INT)
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
  if (tl_type_of(tl_top(TL)) == TL_STR)
    printf("%s", tl_to_str(tl_top(TL)));
  else
    tl_fdebug_obj(stdout, tl_top(TL));
  for (int i = 1; i < argc; i++)
  {
    tl_pop(TL);
    printf("\t");
    if (tl_type_of(tl_top(TL)) == TL_STR)
      printf("%s", tl_to_str(tl_top(TL)));
    else
      tl_fdebug_obj(stdout, tl_top(TL));
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
  tl_register_func(TL, &op_eq, "=", 2, 1);
  tl_fdebug_state(stdout, TL);
  tl_register_func(TL, &op_add, "+", 2, 4);
  tl_register_func(TL, &print, "print", 2, 255);
  tl_register_func(TL, &_debug_state, "_debug_state", 0, 255);
}


