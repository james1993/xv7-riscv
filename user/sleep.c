#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int count;

  if (argc == 1)
    write(1, "Error: no argument\n", 19);

  count = atoi(argv[1]);
  sleep(count);

  exit(0);
}
