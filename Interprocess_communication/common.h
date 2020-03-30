/*
 * Operating Systems {2INCO} Practical Assignment
 * Interprocess Communication
 *
 * STUDENT_NAME_1 Lucas Giordano 1517317
 * STUDENT_NAME_2 Auguste Lefevre 1517600
 *
 * Contains definitions which are commonly used by the farmer and the workers
 *
 */

//We include the librabry of uint128
#include "uint128.h"
#include <stdio.h>


#ifndef COMMON_H
#define COMMON_H


#define MAX_MESSAGE_LENGTH    6 // maximum size for any message in the tests (6 chars)
#define NO_KEY '\0' // represent the null string
typedef unsigned int bool_t; // define the boolean type
// Define the boolean value true and false to 1 and 0
#define TRUE 1
#define FALSE 0

/**
 * @brief      Structure that represents a request message. This type of message is send to the worker from the farmer
 *
 * @param work_id : id of the job : all job related to the same md5 hash have the same id
 * @param kill_request : say if yes or not the worker should kill himself
 * @param first_letter : the first letter of all the string that the worker will have to test
 * @param start_alphabet : the first letter of our alphabet
 * @param end_alphabet : the last letter of our alphabet
 * @param md5s : the md5s hash value which correspond to the password we want to find
 */
typedef struct {
    unsigned int work_id;
    bool_t kill_request;
    char first_letter;
    char start_alphabet;
    char end_alphabet;
    uint128_t md5s;
} MQ_REQUEST_MESSAGE;


/**
 * @brief      Structure that represens a response message. This type of message is send to the farmer from the worker
 *
 * @ack_kill_request : boolean set to TRUE if this messages represents an acknowleadge of a previously recieved kill_request
 * @work_id : same as for MQ_REQUEST_MESSAGE
 * @key : the password (in text) that correspond to the md5s hash sent.
 */
typedef struct {
    bool_t ack_kill_request;
    unsigned int work_id;
    char key[MAX_MESSAGE_LENGTH+1];
} MQ_RESPONSE_MESSAGE;


#endif
