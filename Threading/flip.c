/* 
 * Operating Systems {2INCO} Practical Assignment
 * Threaded Application
 *
 * STUDENT_NAME_1  Lucas Giordano 1517317
 * STUDENT_NAME_2  Auguste Lefevre (1517600)
 *
 * Grading:
 * Students who hand in clean code that fully satisfies the minimum requirements will get an 8. 
 * Extra steps can lead to higher marks because we want students to take the initiative. 
 * Extra steps can be, for example, in the form of measurements added to your code, a formal 
 * analysis of deadlock freeness etc.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>          // for perror()
#include <pthread.h>
#include <string.h>
#include <assert.h>
#include "uint128.h"
#include "flip.h"


#define N_BITS 128 
#define START_INDEX 2 
#define BUFFER_SIZE ((NROF_PIECES/N_BITS) + 1)
#define MAX_SIZE_NUMBERS ((NROF_PIECES/NROF_THREADS) +1)
#define END_NUMBERS (-1)
#define INTIAL_VALUE 0

typedef int* numbers_pt;
typedef int* parameters_t;

#define BITMASK(n)          (((uint128_t) 1) << (n))          				  // create a bitmask where bit at position n is set
#define BIT_IS_SET(v,n)     (((v) & BITMASK(n)) == BITMASK(n))				  // check if bit n in v is set
#define BIT_SET(v,n)        ((v) =  (v) |  BITMASK(n))  	  				  // set bit n in v
#define BIT_CLEAR(v,n)      ((v) =  (v) & ~BITMASK(n))		         		  // clear bit n in v
#define BIT_XOR(v,n)        (BIT_IS_SET(v,n) ? BIT_CLEAR(v,n) : BIT_SET(v,n)) // xor bit n in v

// This two methods allow us to know the exact place of a bit in the buffer
#define index_in_buffer(n) ((n)/N_BITS)
#define bit_in_buffer(n)   ((n)%N_BITS)

//================================================================================
//========================== GLOBAL VARIABLES ====================================
//================================================================================

static pthread_mutex_t      mutex[BUFFER_SIZE]; // declare a mutex array

//================================================================================
//================================ FUNCTIONS =====================================
//================================================================================
/**
 * @brief      Function which create all paramters for each thread depending the number of threads and the number of pieces in the game. 
 *             In order to create threads with equivalent amount of work we decided to divide the work this way : 
 *             If there is X pieces and N threads then thread i (i in 0 to N-1) will take care of pieces/number a = i + j*N with j = 0,1.... while a < X
 *
 * @param[in]  parameters  the data structure where we stock parameters
 */
void create_parameters(parameters_t parameters){
	// first we initialize the array that will countains the different parameters for each thread 
	memset(parameters, END_NUMBERS, sizeof(int)*NROF_THREADS * (MAX_SIZE_NUMBERS +1 ));

	// for all pieces
	for (int i=START_INDEX; i <= NROF_PIECES; i++) {
		// we find which threads should take care of this number (using method described in @brief)
		int t_index = (i-START_INDEX) % NROF_THREADS;
		int index_in_t = (i-START_INDEX)/NROF_THREADS;
		// and add this number at the correct place
		parameters[t_index*(MAX_SIZE_NUMBERS +1)+ index_in_t] =  i;
	}
}

/**
 * @brief      Method that will check all multipes of each number on the 'array' passed as argument.
 *             For each multiples we flip the correspondant bit on the buffer.
 *
 * @param      void_numbers  generic pointer that point to an array which countains the different number
 *
 * @return     generic pointer
 */
void* flip_numbers(void* void_numbers){

	//first we cast the generic pointer to acces the different number
	numbers_pt numbers = void_numbers;

	//then for each number
	for (numbers_pt n = numbers ; (*n)!= END_NUMBERS;++n){
		//and for each multiples of thus number
		for(unsigned int multiple = *n; multiple<=NROF_PIECES; multiple += *n){
			//we find the correct bit to flip (which bit index in which part of the buffer)
			unsigned int index = index_in_buffer(multiple);
			unsigned int bit   = bit_in_buffer(multiple);
			// we lock the mutex and check that the method has been correctly executed
			assert(pthread_mutex_lock(mutex+index)==0);

			////////////////////////////////////////////////////////////////
			// Here we are in the critical section (protected by a mutex) //
			////////////////////////////////////////////////////////////////
			
			//first we get the correct part of the buffer
			uint128_t v = buffer[index];
			//then we flip the bit using an XOR operation
			BIT_XOR(v,bit);
			//then we actualize the buffer
			buffer[index] = v;
			//finally we unlock the mutex and check that the method has been correctly executed
			assert(pthread_mutex_unlock(mutex+index)==0);

			/////////////////////////////////
			// End of the critical section //
			/////////////////////////////////
			}
		}
	// terminates the calling thread
	pthread_exit(0);
	}

/**
 * @brief      Creates threads
 */
void create_threads(void){
	// first we allocated the requested memory to stock the different paramaters use by the different threads
	parameters_t parameters = malloc(sizeof(int) * (MAX_SIZE_NUMBERS + 1)*NROF_THREADS);
	// then we initialize thus parameters calling 'create_parameters'
	create_parameters(parameters);
	
	// now we create all threads with the routine 'flip_umbers' and their own parameters
	// we also check that the function pthread_create has been executed correctly by checking the return value
	pthread_t   thread_id[NROF_THREADS];
	for (int i = 0 ; i < NROF_THREADS ; ++i){
		assert(pthread_create(thread_id + i, NULL, flip_numbers, (parameters+i*(MAX_SIZE_NUMBERS+1)))==0);
	}

	// here we join all the threads using the 'pthread_join' method and checking the return value  
	for (int i = 0; i < NROF_THREADS; i++) {
		assert(pthread_join(thread_id[i], NULL)==0);
	}

	//finally we free the memory used to stock the different parameters
	free(parameters);
}
void print_result(void){
	for (int index_buffer = 0 ; index_buffer < BUFFER_SIZE ; ++index_buffer){
		uint128_t elem = buffer[index_buffer];
		for (int i = 0; i < N_BITS ; ++i){
			unsigned int output_number = index_buffer*N_BITS+i;
			if (output_number <= NROF_PIECES && output_number > 0 && !BIT_IS_SET(elem,i)) printf("%d\n",output_number);
			}
		}
	}

/**
 * @brief      Main function of the class flip.c
 *
 * @return     0 if the programm has been executed correctly, 1 otherwise
 */
int main (void){
	//initialize buffer with the correct size and with INITIAL_VALUE at each bit.
	memset(buffer, INTIAL_VALUE, sizeof(sizeof(uint128_t)*BUFFER_SIZE));

	//create BUFFER_SIZE mutex semaphores
	for (int i = 0 ; i < BUFFER_SIZE ; ++i){
		  assert(pthread_mutex_init(mutex+i, NULL)==0);
		}

	//call create_threads to do the job
	create_threads();

	//finally print the result
    print_result();
    
    //destroy BUFFER_SIZE mutex semaphores
	for (int i = 0 ; i < BUFFER_SIZE ; ++i){
		  assert(pthread_mutex_destroy(mutex+i)==0);
		}
    return (0);
}
