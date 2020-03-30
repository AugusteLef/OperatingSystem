/* 
 * Operating Systems {2INCO} Practical Assignment
 * Condition Variables Application
 *
 * STUDENT_NAME_1 Lucas Giordano 1517317
 * STUDENT_NAME_2 Auguste Lefevre 1517600
 *
 * Grading:
 * Students who hand in clean code that fully satisfies the minimum requirements will get an 8. 
 * Extra steps can lead to higher marks because we want students to take the initiative. 
 * Extra steps can be, for example, in the form of measurements added to your code, a formal 
 * analysis of deadlock freeness etc.
 */
 
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include "prodcons.h"

static ITEM buffer[BUFFER_SIZE];

static void rsleep (int t);			// already implemented (see below)
static ITEM get_next_item (void);	// already implemented (see below)

//==============================================================================================
//================================== global variables ==========================================
//==============================================================================================
static int next_number_to_push  = 0;
static int next_number_to_print = 0;
static pthread_cond_t  full_condition    = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  empty_condition   = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  turn_condition    = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t buffer_mutex      = PTHREAD_MUTEX_INITIALIZER;
//==============================================================================================
//================================== buffer access API =========================================
//==============================================================================================
typedef enum {
	EMPTY, MID, FULL
	} BUFFER_STATE;
//******** variables **********
static  int head  = 0;
static  int tail  = 0;

static  BUFFER_STATE state = EMPTY;
static  BUFFER_STATE previous_state = EMPTY;
//******** functions **********
/**
 * @brief determines if the buffer is full
 */
#define IS_FULL ((state) == FULL)
/**
 * @brief determines if the buffer is empty
 */
#define IS_EMPTY ((state) == EMPTY)
/**
 * @brief      Function that adds a item into the array 
 *
 * @param      buffer : pointer buffer in which we want to add the item
 * @param 	   v      : item
 */
void buffer_push(ITEM* buffer, ITEM v){
	previous_state = state ;
	buffer[tail] = v;
	tail = (tail + 1)%BUFFER_SIZE;
	if (tail == head) { state = FULL;}
	else              { state = MID; }
	}
/**
 * @brief      Function that gets a item from the array 
 *
 * @param      buffer : pointer buffer from which we want to get the item
 * @return 	   v      : item to return
 */
ITEM buffer_get(ITEM* buffer){
	previous_state = state ;
	ITEM v = buffer[head];
	head = (head +1)%BUFFER_SIZE;
	if (head == tail) { state = EMPTY;}
	else 			  { state = MID;  }
	return v;
	}
//==============================================================================================
//====================================== OPTIMIZATION ==========================================
//==============================================================================================
//******** variables **********
static int waiting_items[NROF_PRODUCERS];
//******** functions **********
/**
 * @brief      This function is used in a producer to know if we need signaling or not. This is 
 * 			   used to reduce as much signaling as possible due to the following observation :
 * 					we only need o signal the other threads if the next number has already 
 * 					been generated by another thread (and is thus stored in the waiting_item
 * 					array)
 * 				
 * 				=> we can thus do a linear search in the array to perform this check
 *
 * @param      item : item for which we want to check if signaling is needed
 * @return     bool : true if item needs signaling
 */
bool need_signal(int item){
	for (int i = 0 ; i < NROF_PRODUCERS; ++i){
		if (waiting_items[i] == item) return true;
	}
	return false;
}
//==============================================================================================
//======================================= main functions =======================================
//==============================================================================================
/* producer thread */
/**
 * @brief      Act as a producer. Put items in the buffer[] (in ascending order) and take care of any eventual deadlocks
 *
 * @param      arg  Generic Pointer (not used) 
 * @return     { NULL }
 */
static void * producer (void * arg){
	int const process_id = *((int*) arg); // gets thread id used to index : waiting_items array
	ITEM i = -1;                          // intializes i 
    while ((i = get_next_item()) < NROF_ITEMS){                             //      get the new item 

        rsleep (100);	                                                    //      simulating all kind of activities...
		
		if (i == NROF_ITEMS) {return 0;}  //exit normaly
        pthread_mutex_lock(&buffer_mutex);                                  //      mutex-lock;
        while (i != next_number_to_push) {
			waiting_items[process_id] = i; // waiting item for this process is stored
			pthread_cond_wait(&turn_condition, &buffer_mutex);              //      wait-cv;
		}
		while (IS_FULL) pthread_cond_wait(&full_condition, &buffer_mutex);  //      wait-cv;
        //we know that there is at least one empty slot in the buffer
        // ***************************** start of critical section *****************************************
        buffer_push(buffer, i);
        next_number_to_push+=1;
       
        if (need_signal(i+1)) pthread_cond_broadcast(&turn_condition);
        if (previous_state == EMPTY) pthread_cond_signal(&empty_condition); //if the buffer was previously empty
																			//the consumer thread may want to be
																			//woken up.
        // ***************************** end of critical section ******************************************
		pthread_mutex_unlock(&buffer_mutex);                                //      mutex-unlock;
    }
	return (NULL);
}

/* consumer thread */
/**
 * @brief      Act as a Consumer. Get the next item from the buffer[] and take care of any eventual deadlock
 *
 * @param      arg   Generic pointer 
 *
 * @return     { NULL }
 */
static void * consumer (void * arg){
    while (next_number_to_print < NROF_ITEMS){
		
		rsleep (100);	                                                       	  //      simulating all kind of activities...
		
		pthread_mutex_lock (&buffer_mutex);                                       //      mutex-lock;
		while (IS_EMPTY) pthread_cond_wait(&empty_condition, &buffer_mutex);      //      wait-cv;
        //we know that there is at least one empty slot in the buffer
        // ***************************** start of critical section *****************************************
        printf("%d\n",buffer_get(buffer));
        next_number_to_print +=1;
        if (previous_state == FULL) pthread_cond_signal(&full_condition);         //      possible-cv-signals;
        //***************************** end of critical section ******************************************
		pthread_mutex_unlock (&buffer_mutex);                                     //      mutex-unlock;
    }
	return (NULL);
}


/**
 * @brief      Main function of the programm. Create semaphores; consumer and producer threads and finally join all the threads
 *
 * @return     { description_of_the_return_value }
 */
int main (void){
	pthread_t   thread_id[NROF_PRODUCERS+1];
    //********** create consumer thread **************
	assert(pthread_create(thread_id+NROF_PRODUCERS, NULL, consumer, NULL)==0);
    
    //********** create producer threads **************
    int* param = malloc(sizeof(int)*NROF_PRODUCERS); 
    for (int i = 0 ; i < NROF_PRODUCERS; ++i){
		param[i] = i; //thread should have a unique id between 0 and NROF_PRODUCERS-1 to access waiting_items
		assert(pthread_create(thread_id + i, NULL, producer, param + i)==0);
		}
	
	// here we join all the threads using the 'pthread_join' method and checking the return value  
	for (int i = 0; i < NROF_PRODUCERS+1; i++) {
		assert(pthread_join(thread_id[i], NULL)==0);
	}
	free(param); //free param
    return (0);
}

/*
 * rsleep(int t)
 *
 * The calling thread will be suspended for a random amount of time between 0 and t microseconds
 * At the first call, the random generator is seeded with the current time
 */
static void 
rsleep (int t)
{
    static bool first_call = true;
    
    if (first_call == true)
    {
        srandom (time(NULL));
        first_call = false;
    }
    usleep (random () % t);
}


/* 
 * get_next_item()
 *
 * description:
 *		thread-safe function to get a next job to be executed
 *		subsequent calls of get_next_item() yields the values 0..NROF_ITEMS-1 
 *		in arbitrary order 
 *		return value NROF_ITEMS indicates that all jobs have already been given
 * 
 * parameters:
 *		none
 *
 * return value:
 *		0..NROF_ITEMS-1: job number to be executed
 *		NROF_ITEMS:		 ready
 */
static ITEM
get_next_item(void){
    static pthread_mutex_t	job_mutex	= PTHREAD_MUTEX_INITIALIZER;
	static bool 			jobs[NROF_ITEMS+1] = { false };	// keep track of issued jobs
	static int              counter = 0;    // seq.nr. of job to be handled
    ITEM 					found;          // item to be returned
	
	/* avoid deadlock: when all producers are busy but none has the next expected item for the consumer 
	 * so requirement for get_next_item: when giving the (i+n)'th item, make sure that item (i) is going to be handled (with n=nrof-producers)
	 */
	pthread_mutex_lock (&job_mutex);

    counter++;
	if (counter > NROF_ITEMS){
	    // we're ready
	    found = NROF_ITEMS;
	}
	else{
	    if (counter < NROF_PRODUCERS)  {
	        // for the first n-1 items: any job can be given
	        // e.g. "random() % NROF_ITEMS", but here we bias the lower items
	        found = (random() % (2*NROF_PRODUCERS)) % NROF_ITEMS;
	    }
	    else{
	        // deadlock-avoidance: item 'counter - NROF_PRODUCERS' must be given now
	        found = counter - NROF_PRODUCERS;
	        if (jobs[found] == true) {
	            // already handled, find a random one, with a bias for lower items
	            found = (counter + (random() % NROF_PRODUCERS)) % NROF_ITEMS;
	        }    
	    }
	    
	    // check if 'found' is really an unhandled item; 
	    // if not: find another one
	    if (jobs[found] == true) {
	        // already handled, do linear search for the oldest
	        found = 0;
	        while (jobs[found] == true){
                found++;
            }
	    }
	}
    jobs[found] = true;
			
	pthread_mutex_unlock (&job_mutex);
	return (found);
}
