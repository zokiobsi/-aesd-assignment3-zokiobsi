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
    // This function waits, obtains mutex, waits, releases mutex as described by thread_data structure in threading.h
  
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    
    usleep(thread_func_args->wait_to_obtain_ms * 1000); // wait for x ms, convert to us for usleep()
    pthread_mutex_lock(thread_func_args->lock); //lock the mutex
    
    usleep(thread_func_args->wait_to_release_ms * 1000); //wait for x ms, convert to us for usleep()
    thread_func_args->thread_complete_success = true; //set the complete to true before unlock for thread safety 
    pthread_mutex_unlock(thread_func_args->lock); //unlock the mutex
    
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */

    struct thread_data *data = malloc(sizeof(struct thread_data));
    data->lock = mutex;
    data->wait_to_obtain_ms = wait_to_obtain_ms;
    data->wait_to_release_ms = wait_to_release_ms;
    data->thread_complete_success = false;

    if (!data){
        printf("malloc failed");
        return false;
    }

    int rc = pthread_create(thread, NULL, threadfunc, data);
    
    if (rc){
        free(data);
        printf("thread creation failed");
        return false;
    }

    return true;
}

