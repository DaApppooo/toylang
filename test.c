#include "tl.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
  TLState* state;
  char* own_code;
} BuildResult;

BuildResult build_bytecode_from_file(const char* filename)
{
  BuildResult res = (BuildResult){.state = NULL, .own_code = NULL};
  FILE* f = fopen(filename, "r");
  if (!f)
  {
    fprintf(stderr, "ERROR: Failed to open file '%s'\n", filename);
    return res;
  }
  fseek(f, 0, SEEK_END);
  const long len = ftell(f);
  res.own_code = calloc(len+1, sizeof(char));
  fseek(f, 0, SEEK_SET);
  fread(res.own_code, sizeof(char), len, f);
  fclose(f);
  
  res.state = tl_new_state();
  tl_load_builtins(res.state);
  tl_load_openlib(res.state->scope);
  if (tl_errored())
  {
    fprintf(stderr, "\e[0;31m%s\e[0m", tl_errored());
    res.state = NULL;
    return res;
  }
  tl_parse_to_bytecode(res.state, res.own_code);
  return res;
}


int main(int argc, const char* argv[])
{
  if (argc == 1)
  {
    // Interactive toylang shell
    puts("Interactive Toylang Shell v0.0.1 [inbuild]");
    return 0;
  }
  else if (argc == 4)
  {
    if (strcmp(argv[1], "-s") == 0)
    {
      BuildResult br = build_bytecode_from_file(argv[2]);
      int status = 0;
      if (br.state == NULL)
        return 1;
      size_t len = 0;
      char* data = tl_serialize(br.state, &len);
      if (strcmp(argv[3], "-") == 0)
      {
        if (fwrite(data, len, 1, stdout) != len)
        {
          fprintf(stderr, "Failed to write %lu bytes.", len);
          status = 1;
          goto DASH_S_END;
        }
      }
      else
      {
        FILE* o = fopen(argv[3], "wb");
        if (!o)
        {
          fprintf(stderr, "Failed to open output file '%s'.\n", argv[3]);
          status = 1;
          goto DASH_S_END;
        }
        if (fwrite(data, len, 1, o) != len)
        {
          fprintf(stderr, "Failed to write %lu bytes.", len);
          status = 1;
          goto DASH_S_END;
        }
        fclose(o);
      }
DASH_S_END:
      tl_destroy(br.state);
      free(br.own_code);
      return status;
    }
    else
    {
      printf("Unknown arguments %s %s %s", argv[1], argv[2], argv[2]);
      return 1;
    }
  }
  else if (argc == 2)
  {
    BuildResult br = build_bytecode_from_file(argv[1]);
    if (br.state == NULL)
      return 1;
    if (tl_errored())
    { fprintf(stderr, "\e[0;31m%s\e[0m", tl_errored()); return 1; }
    tl_run_bytecode(br.state);
    if (tl_errored())
    { fprintf(stderr, "\e[0;31m%s\e[0m", tl_errored()); return 1; }
    tl_destroy(br.state);
    free(br.own_code);
  }
  else
  {
    puts("Syntax: toylang [script.tl]\n");
    return 1;
  }
  return 0;
}

