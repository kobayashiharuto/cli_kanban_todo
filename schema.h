#define LISTEN 0
#define DISPOSE 1
#define ADD_TASK 2
#define UPDATE_TASK 3
#define DELETE_TASK 4
#include "task.h"

typedef struct
{
  int type;
  int index;
  Task task;
} Request;

typedef struct
{
  Task tasks[MAX_TASK_COUNT];
  int task_count;
} Responce;