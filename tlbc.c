#include "tl.h"
#include <assert.h>

int tl_run_bytecode_ex(TLScope *TL, int len, int depth,
                       int param_count) {
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
      tl_set_name_ex(
        TL, tlbc_arg(code),
        TL->global->consts[fnh_register]
      );
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
        int start = TL->bc_pos+1;
        int i = TL->bc_pos+1;
        TLScope* child = tl_new_scope_ex(
          TL->global, TL, TL->stack_cap,
          TL->max_name_count, i
        );
        if (tlbc_arg(bc[i]) != 0)
          i += tlbc_arg(bc[i]);
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
              bc[start] = CODE(tlbc_opcode(bc[start]), i - start);
              child->bc_pos = start+1;
              tl_run_bytecode_ex(child, i, depth, 0);
              start = i;
              if (tlbc_arg(bc[i]))
                i += tlbc_arg(bc[i]) - 1;
            }
            i++;
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
