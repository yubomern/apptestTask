#define _GNU_SOURCE  // To get gettid()
#include <pthread.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

static volatile pid_t child = -2;

void *sleepnprint(void *arg)
{
  printf("Process %d, Task %d:%s starting up...\n", getpid(), gettid(), (char *) arg);

  // This is not the best way to synchronize threads but this works here:
  // once the main thread returns from fork(), child = pid of child process
  // (i.e. != -2)
  while (child == -2) {sleep(1);} /* Later we will use condition variables */

  printf("Process %d, Task %d:%s finishing...\n", getpid(), gettid(), (char*)arg);

  return NULL;  
}

int main() {

  pthread_t tid1, tid2;
  pthread_create(&tid1,NULL, sleepnprint, "New Thread One");
  pthread_create(&tid2,NULL, sleepnprint, "New Thread Two");
 
  child = fork();

  // In father process: child = child process pid
  // In child process: child = 0

  if (child == 0) {

    // This is the child process

    printf("%d:%s\n",getpid(), "Child process finished");

    exit(0);
  }

  // Father process

  printf("%d:%s\n",getpid(), "fork()ing complete");
  sleep(3);

  printf("%d:%s\n",getpid(), "Main thread finished");

  pthread_exit(NULL);
  return 0; /* Never executes */
}