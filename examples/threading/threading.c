#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

   

    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // Wait before obtaining mutex lock.
    unsigned int usecs;
    int result;
    usecs = thread_func_args->wait_to_obtain_ms * 1000;
    printf("***threadfunc sleeping for %d ms before obtaining mutex lock.\n", thread_func_args->wait_to_obtain_ms);
    result = usleep(usecs);
    if (result == 0) printf("sleep all okay.\n");
    // obtain mutex lock.
    printf("*** threadfunc get mutex lock.\n");
    result = pthread_mutex_lock(thread_func_args->mutex);
    if (result != 0) {
        thread_func_args->thread_complete_success = false;
    }
    usecs = thread_func_args->wait_to_release_ms * 1000;
    printf("*** threadfunc sleeping for %d ms before releasing mutex lock.\n", thread_func_args->wait_to_release_ms);
    result = usleep(usecs);
    if (result == 0) printf("sleep2 all okay.\n");
    pthread_mutex_unlock(thread_func_args->mutex);
    thread_func_args->thread_complete_success = true;
    
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    
    struct thread_data* threadData = malloc(sizeof(struct thread_data));
    threadData->wait_to_obtain_ms = wait_to_obtain_ms;
    threadData->wait_to_release_ms = wait_to_release_ms;
    threadData->mutex = mutex;
    /* int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start_routine) (void *), void *arg); */
    int didcreate = pthread_create(thread, NULL, threadfunc, threadData);
    //pthread_join(*thread, NULL);
    if (didcreate == 0) {
        return true;
    } else {
        return false;
    }
    //return threadData->thread_complete_success;
}

