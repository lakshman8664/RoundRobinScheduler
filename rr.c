#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef uint32_t u32;
typedef int32_t i32;

struct process
{
  u32 pid;
  u32 arrival_time;
  u32 burst_time;

  TAILQ_ENTRY(process) pointers;

  /* Additional fields here */

  u32 wait_time;
  u32 response_time;
  u32 remaining_time;
  bool in_queue;
  bool started;

  /* End of "Additional fields here" */

};


TAILQ_HEAD(process_list, process);

u32 next_int(const char **data, const char *data_end)
{
  u32 current = 0;
  bool started = false;
  while (*data != data_end)
  {
    char c = **data;

    if (c < 0x30 || c > 0x39)
    {
      if (started)
      {
        return current;
      }
    }
    else
    {
      if (!started)
      {
        current = (c - 0x30);
        started = true;
      }
      else
      {
        current *= 10;
        current += (c - 0x30);
      }
    }

    ++(*data);
  }

  printf("Reached end of file while looking for another integer\n");
  exit(EINVAL);
}

u32 next_int_from_c_str(const char *data)
{
  char c;
  u32 i = 0;
  u32 current = 0;
  bool started = false;
  while ((c = data[i++]))
  {
    if (c < 0x30 || c > 0x39)
    {
      exit(EINVAL);
    }
    if (!started)
    {
      current = (c - 0x30);
      started = true;
    }
    else
    {
      current *= 10;
      current += (c - 0x30);
    }
  }
  return current;
}

void init_processes(const char *path,
                    struct process **process_data,
                    u32 *process_size)
{
  int fd = open(path, O_RDONLY);
  if (fd == -1)
  {
    int err = errno;
    perror("open");
    exit(err);
  }

  struct stat st;
  if (fstat(fd, &st) == -1)
  {
    int err = errno;
    perror("stat");
    exit(err);
  }

  u32 size = st.st_size;
  const char *data_start = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data_start == MAP_FAILED)
  {
    int err = errno;
    perror("mmap");
    exit(err);
  }

  const char *data_end = data_start + size;
  const char *data = data_start;

  *process_size = next_int(&data, data_end);

  *process_data = calloc(sizeof(struct process), *process_size);
  if (*process_data == NULL)
  {
    int err = errno;
    perror("calloc");
    exit(err);
  }

  for (u32 i = 0; i < *process_size; ++i)
  {
    (*process_data)[i].pid = next_int(&data, data_end);
    (*process_data)[i].arrival_time = next_int(&data, data_end);
    (*process_data)[i].burst_time = next_int(&data, data_end);
  }

  munmap((void *)data, size);
  close(fd);
}



int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    return EINVAL;
  }
  struct process *data;
  u32 size = 0;
  init_processes(argv[1], &data, &size); // initializes the size

  u32 quantum_length = next_int_from_c_str(argv[2]);

  struct process_list list;
  TAILQ_INIT(&list);

  u32 total_waiting_time = 0;
  u32 total_response_time = 0;

  /* Your code here */
  u32 curr_time = 0;  
  u32 exec_time = 0;
  struct process *curr_proc = NULL;  

  struct process_list temp_list; 
  TAILQ_INIT(&temp_list);

  // initial queue creation
  for (u32 i = 0; i < size; ++i) {
      data[i].started = false;  // Mark as not started
      data[i].in_queue = false;
      data[i].wait_time = 0;
      if (data[i].arrival_time <= 0){
        data[i].remaining_time = data[i].burst_time;  // Set remaining time equal to burst time initially
        TAILQ_INSERT_TAIL(&list, &data[i], pointers);
        data[i].in_queue = true;
      }
  }

  // RR loop
  while (!TAILQ_EMPTY(&list)) {
    curr_proc = TAILQ_FIRST(&list);
    TAILQ_REMOVE(&list, curr_proc, pointers);

      // calculate response time for the process
      if (curr_time >= curr_proc->arrival_time){
        if (!curr_proc->started) {
            curr_proc->wait_time = curr_time - curr_proc->arrival_time; // calc wait time
            total_response_time += curr_time - curr_proc->arrival_time;
            curr_proc->started = true; // mark flag started
        }
      }

      // calc exec time 
      if (curr_proc->remaining_time < quantum_length)
        exec_time = curr_proc->remaining_time;
      else
        exec_time = quantum_length;

      // update wait time for other processes 
      struct process *other_proc;
      TAILQ_FOREACH(other_proc, &list, pointers) {
          if (other_proc != curr_proc) 
              other_proc->wait_time += exec_time;
      }

      curr_proc->remaining_time -= exec_time; // update remaining time for process
      curr_time += exec_time; // update curr time 

      for (u32 i = 0; i < size; ++i) {
          if (!data[i].in_queue && data[i].arrival_time <= curr_time) {
              data[i].wait_time = 0;
              data[i].response_time = 0;
              data[i].remaining_time = data[i].burst_time;
              data[i].in_queue = true;


              bool added;
              struct process *iter;
              TAILQ_FOREACH(iter, &temp_list, pointers) {
                  if (data[i].arrival_time < iter->arrival_time) {
                      // Insert new process before current process in the temp list
                      added = true;
                      TAILQ_INSERT_BEFORE(iter, &data[i], pointers);
                      break;
                  }
              }
              if (!added) {
                  TAILQ_INSERT_TAIL(&temp_list, &data[i], pointers);
              }
          }
      }

      while (!TAILQ_EMPTY(&temp_list)) {
          struct process *temp_process = TAILQ_FIRST(&temp_list);
          TAILQ_REMOVE(&temp_list, temp_process, pointers);
          TAILQ_INSERT_TAIL(&list, temp_process, pointers); //move to main queue 
      }


      // If process  finished execution, calc waiting time 
      if (curr_proc->remaining_time <= 0) {
          total_waiting_time += curr_proc->wait_time;
          // printf("Waiting time updated to: %u\n", total_waiting_time);
          // printf("Process finished!\n");
      } 
      else {
          TAILQ_INSERT_TAIL(&list, curr_proc, pointers);
          // printf("Process being inserted back into queue\n");
      }
  }

  printf("Average waiting time: %.2f\n", (float)total_waiting_time / (float)size);
  printf("Average response time: %.2f\n", (float)total_response_time / (float)size);

  free(data);
  return 0;
}