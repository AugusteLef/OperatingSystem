/*
 * Operating Systems {2INCO} Practical Assignment
 * Interprocess Communication
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
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>          // for perror()
#include <unistd.h>         // for getpid()
#include <mqueue.h>         // for mq-stuff
#include <time.h>           // for time()
#include <complex.h>

#include <math.h> // TO USE MATHS FUNCTION

#include "common.h"
#include "md5s.h"

static void rsleep (int t);


/**
 * @brief      methode that generates keys (a password) using simple key as basis, compute the hash of this key,
 *             compare it to the hash we are looking for and if they are the same return
 *             the key. Otherwise create a new and call itself(recursion) until it find te correct key or
 *             the key exceed the max # of char
 *
 * @param  current_size The actual size of the key (# of char)
 * @param  key          The basis key (password), from wich we will derive other keys
 * @param  alphabet     The alphabet of all char that could be in the key
 * @param  hash         The hash we are looking for
 * @return              The key which correspond to the hash value (when encrypt using md5s system), NULL if the key does not correspond
 */
char* generateKey(size_t current_size, char* key, char* alphabet, uint128_t hash) {

    //  test if the size of the key does not exceed the maximum size of a key.
    if (current_size > MAX_MESSAGE_LENGTH) return NULL;
    //  test if the hash value of the key corrspond to the hash value we are looking for
    if (md5s(key, current_size) == hash) return key;
    // Test if the size of the key is equal to the maximum size allowed for a key
    if(current_size == MAX_MESSAGE_LENGTH) return NULL;

    // for loop that will generate all the key of lenght curren_size + 1 that have as prefix the key and as suffix a letter of the alphabet.
    // Then we call generateKey with this new key to chekc if it correspond to the hash value.
    for (char* newChar = alphabet; (*newChar) != '\0'; ++newChar){
        //Create the new Key
        key[current_size] = *newChar;
        size_t new_size = current_size + 1;
        memset(key+new_size,0,(MAX_MESSAGE_LENGTH-new_size+1)*sizeof(char));

        //call generateKey with the new key
        char* output_key = generateKey(new_size, key, alphabet,hash);

        //If output_key is non NULL it means we have a find a key such that the hash is equal to the hash we are looking for
        if (output_key != NULL) {
            return output_key;
        }
    }
    return NULL;
}

/**
 * @brief      Methode that creates a MQ_RESPOND_MESSAGE to send to the farmer
 *
 * @param[in]  mqr   The mq_request_message wich we want to respond
 *
 * @return     a response message of type MQ_RESPOND_MESSAGE
 */
MQ_RESPONSE_MESSAGE construct_response(MQ_REQUEST_MESSAGE* mqr){

    //First we compute the alphabet using the start_/end_ alphabet variable of the request message
    int ALPHABET_NROF_CHAR = (mqr->end_alphabet - mqr->start_alphabet + 1);
    char alphabet[ALPHABET_NROF_CHAR + 1];
    memset(alphabet, 0, sizeof(alphabet));
    for (int j = 0; j < ALPHABET_NROF_CHAR ; ++j){
        alphabet[j] = mqr->start_alphabet + j;
    }


    //Try all possibilities of key to find the correct one (if possible) using the generateKey function
    char key[MAX_MESSAGE_LENGTH+1];
    memset(key, 0, sizeof(key));
    key[0] = mqr->first_letter; //the first letter is in all combination
    char* output_key = generateKey(1, key, alphabet, mqr->md5s);

    // Create the response message with the correct work_id of the worker and the key
    MQ_RESPONSE_MESSAGE response;
    memset(&response, 0, sizeof(MQ_RESPONSE_MESSAGE));
    response.ack_kill_request = FALSE;
    response.work_id = mqr->work_id;
    if(output_key !=  NULL) {
        strncpy(response.key, output_key, MAX_MESSAGE_LENGTH+1);
    }
    return response;
}





/*
 * rsleep(int t)
 *
 * The calling thread will be suspended for a random amount of time
 * between 0 and t microseconds
 * At the first call, the random generator is seeded with the current time
 */
static void rsleep (int t)
{
    static bool first_call = true;
    
    if (first_call == true)
    {
        srandom (time (NULL) % getpid ());
        first_call = false;
    }
    usleep (random() % t);
}

/**
 * Main function of a worker.
 *
 * @param  argc
 * @param  argv
 * @return
 */
int main (int argc, char * argv[]) {

    if (argc != 3){ //should have the program itself and the names of the queues
        fprintf (stderr, "%s: invalid arguments\n", argv[0]);
    }
    // open the two message queues (whose names are provided in the arguments)
    mqd_t mq_fd_request = mq_open(argv[1], O_RDONLY);
    mqd_t mq_fd_response = mq_open(argv[2], O_WRONLY);
    MQ_REQUEST_MESSAGE req;

    while(TRUE) {
        //- read from a message queue the new job to do
        mq_receive(mq_fd_request, (char *) &req, sizeof (req), NULL);
        //- wait a random amount of time
        rsleep(1000);

        if(req.kill_request) {// if the reqauest message ask the worker to kill himself
            //acknowleadges the kill request to the farmer
            MQ_RESPONSE_MESSAGE kill_rsp;
            kill_rsp.ack_kill_request = TRUE;
            mq_send (mq_fd_response, (char *) &kill_rsp, sizeof (kill_rsp), 0);
            //no more to do -> close queues and exit
            if(mq_close(mq_fd_request) < 0 || mq_close(mq_fd_response) < 0){// We just verify that request
                                                                    //and response message are correctly close
                // if they are not we print an error and exit anormally
                perror("Error : stopping queue");
                exit(1);
            }
            exit(0); // Exit normally, ERR_NONE
        } else {
            //- do the job
            MQ_RESPONSE_MESSAGE rsp = construct_response(&req);
            //- write the results to a message queue
            if (rsp.key[0] != NO_KEY) mq_send (mq_fd_response, (char *) &rsp, sizeof (rsp), 0);

        }
    }
    return (0);
}
