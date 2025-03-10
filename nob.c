#define NOB_IMPLEMENTATION

#include "build/nob.h"

#define BUILD_FOLFER  "build/"
#define SRC_FOLDER    "src/"
#define OBJ_FOLDER    "obj/"

int main(int argc, char **argv)
{
  NOB_GO_REBUILD_URSELF(argc, argv);

  Nob_Cmd cmd = {0};

  if (!nob_mkdir_if_not_exists(BUILD_FOLFER)) return 1;
  if (!nob_mkdir_if_not_exists(OBJ_FOLDER)) return 1;

  nob_cmd_append(&cmd, "gcc", "-pthread");
  nob_cmd_append(&cmd, "-Wall", "-Wextra", "-fPIC", "-c");
  nob_cmd_append(&cmd, SRC_FOLDER"fio.c", "-o", OBJ_FOLDER"fio.o");
  if (!nob_cmd_run_sync(cmd)) return 1;
  
  cmd.count = 0;
  nob_cmd_append(&cmd, "gcc");
  nob_cmd_append(&cmd, "-Wall", "-Wextra", "-shared");
  nob_cmd_append(&cmd, "-o", BUILD_FOLFER"fio.so", OBJ_FOLDER"fio.o");
  if (!nob_cmd_run_sync(cmd)) return 1;
  return 0;
}

