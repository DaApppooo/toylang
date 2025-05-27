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

