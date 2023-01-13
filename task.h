#include "config.h"

typedef struct
{
  char name[TASK_NAME_LEN];
  int status;
  int priority;
} Task;