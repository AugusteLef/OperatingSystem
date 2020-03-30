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
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>         // for execlp
#include <mqueue.h>         // for mq

#include "settings.h"
#include "common.h"

// macro that implement the mathematical min function.
#define min(x,y) ((x)<(y)?(x):(y))

#define STUDENT_NAME_1 "Lucas Giordano"
#define STUDENT_NAME_2 "Auguste Lefevre"
#define NROF_MESSAGES (JOBS_NROF + NROF_KILL_MESSAGES)
#define NROF_WORKERS_REQUIRED min(NROF_WORKERS, JOBS_NROF)
#define NROF_KILL_MESSAGES NROF_WORKERS_REQUIRED

//////////////////////////////////////////
//gobal variables : names of the queues //
//////////////////////////////////////////
char mq_request_name[80];
char mq_response_name[80];

/**
 * @brief      Structure that represent the actual state of our programm (# of request and wich one is actually processing)
 *
 * @param requests : array of the all the request message to do/done
 * @param kill_request : array of kill request to be sent
 * @param indices : array that contains the state of request that have been put the request queue :
 *            indices[2] = 3 means that jobs related to md5 number 2 until the 3rd char
 *            of the alphabet have been put in the request queue
 * @param current_md5 : index of the current md5 to send
 * @param index_kill_request : index of the next kill request to send
 */
typedef struct {
    MQ_REQUEST_MESSAGE requests[MD5_LIST_NROF][ALPHABET_NROF_CHAR];
    MQ_REQUEST_MESSAGE kill_request[NROF_WORKERS_REQUIRED];
    unsigned int indices[MD5_LIST_NROF];
    unsigned int current_md5;
    unsigned int index_kill_request;
    } request_state_t;

    
//================================================================================
//=========================== queue operations ===================================
//================================================================================

/**
 *    @brief      Method that create a queue
 *
 * @param mq_name       the name of the queue
 * @param name          the name
 * @param msgsize       the size of te message
 * @param rw_priviledge priviledge information (read or write)
 * @param mq_fd         queue to be initialize
 * @param attr          attribute of the queue pq_fd to initialize
 */
void create_queue(char* mq_name,char* name, size_t msgsize, int rw_priviledge,
                    mqd_t* mq_fd,struct mq_attr* attr){
    //name
    sprintf(mq_name, "/%s_%s_%d", name, STUDENT_NAME_1, getpid());
    
    attr->mq_maxmsg  = MQ_MAX_MESSAGES; // queue will block if there are too many messages
    attr->mq_msgsize = msgsize;
    
    *mq_fd = mq_open (mq_name, rw_priviledge | O_CREAT | O_EXCL, 0600, attr); //creation of the first queue with read/write priviledge
    if (*mq_fd == (mqd_t) -1) {perror("queue creation failed"); exit (1);}
    }

/**
 * @brief      Closes a queue or exit in case of error
 *
 * @param[in]  mq    queue to be closed
 * @param[in]  name  desallocate the name of the queue in the system
 */
void close_queue(mqd_t mq, const char* name){
    if(mq_close(mq) < 0){
        perror("Error : stopping queue");
        exit(1);
    }
    if(mq_unlink(name) < 0){
        perror("Error : unlinking queue");
        exit(1);
    }
}

/**
 * @brief      Gets the current number of messages in the queue or exit in case of error.
 *
 * @param[in]  mq_fd  The queue
 *
 * @return     the current number of messages in the queue.
 */
long int get_curmsgs(mqd_t mq_fd){
    struct mq_attr     attr;
    if (mq_getattr (mq_fd, &attr) == -1){
        perror ("mq_getattr() failed");
        exit (1);
    }
    return attr.mq_curmsgs;
}
//================================================================================
//=========================== farmer operations ==================================
//================================================================================

/**
 * @brief      Creates a worker or exit in case of error
 *
 * @return     the process identification of the worker
 */
pid_t create_worker(void){
    // create another process
    pid_t pid = fork();
    // check if the creation has been executed correctly, otherwose exit anormally
    if (pid < 0) {
        perror("fork() failed");
        exit (1);
    }
    else if (pid == 0){ //if we are the child process
        //run worker class
        execlp("./worker","./worker", mq_request_name, mq_response_name, NULL);//as required in the assignement we send the names of the queues
        // we should never arrive here...
        perror ("execlp() failed");
        exit(1);
        }
    else return pid;
    }

/**
 * @brief      Creates NROF_WORKERS_REQUIRE child processes
 */
void create_child_processes(void){
    for (size_t i = 0 ; (i < NROF_WORKERS_REQUIRED); ++i){ //only parent should create process
        create_worker();
        }
    }
/**
 * @brief      Sends nrof_requests requests in the queue mq_fd_request
 *
 * @param[in]  mq_fd_request  The mq fd request queue
 * @param      request_state  The request state
 * @param[in]  nrof_requests  The nrof requests we have to send
 */
void send_requests(mqd_t mq_fd_request, request_state_t* request_state, unsigned int nrof_requests){
    // send all requests and update the request state
    for (int i = 0; i < nrof_requests; ++i){ //send nrof_requests jobs
        if (request_state->indices[request_state->current_md5] == ALPHABET_NROF_CHAR){ //if we sent all jobs related
                                                   //to current_md5 -> move to next
                                                   // md5
            request_state->current_md5++;
            }
        MQ_REQUEST_MESSAGE current_request;
        if (request_state->current_md5 == MD5_LIST_NROF){//send kill request if no more md5 to process
            current_request = request_state->kill_request[request_state->index_kill_request];
            request_state->index_kill_request++;
            }
        else { //send normal request from request_state->requests
            unsigned int current_md5 = request_state->current_md5;
            current_request = request_state->requests[current_md5][request_state->indices[current_md5]];
            request_state->indices[current_md5]++;
            }
        mq_send (mq_fd_request, (char *) (&current_request), sizeof(MQ_REQUEST_MESSAGE), 0);
        }
    }

/**
 * @brief      method that manage the responses
 *
 * @param[in]  mq_fd_response  The mq fd response
 * @param[in]  mq_fd_request   The mq fd request
 * @param      request_state   The request state
 * @param      key_found       The key found
 */
void manage_responses(mqd_t mq_fd_response, mqd_t mq_fd_request, request_state_t* request_state, char* key_found){
    unsigned int nbr_worker_left = NROF_WORKERS_REQUIRED;
    MQ_RESPONSE_MESSAGE rsp;
    
    while (nbr_worker_left > 0){
        if (request_state->index_kill_request < NROF_KILL_MESSAGES) //while jobs need to be sent
            send_requests(mq_fd_request, request_state, 1);//this send may block
        
        long int curmsgs = get_curmsgs(mq_fd_response);
        for (int i = 0 ; i< curmsgs; ++i){ //
            mq_receive(mq_fd_response, (char *) &rsp, sizeof (MQ_RESPONSE_MESSAGE), NULL); //this receive will not block
            if (rsp.ack_kill_request == TRUE){
                nbr_worker_left--;
                }
            else {
                strncpy(key_found + (rsp.work_id*(MAX_MESSAGE_LENGTH+1)), rsp.key, MAX_MESSAGE_LENGTH);
                //*** optimization ***
                //if a hash is found -> no need to send other jobs related to the same hash
                request_state->indices[rsp.work_id] = ALPHABET_NROF_CHAR;
                }
            }
        }
    wait(NULL); //waits for all children to exit
}
/**
 * @brief      method that prints to stdout all keys found
 *
 * @param  array: 2d array (size n1 x n2)containing the keys to output.
 * @param  n1   : size of the first dimension of array
 * @param  n2   : size of the second dimension of array
 */
void print_keys_found(char* array, size_t n1, size_t n2){
    for (size_t i = 0 ; i < n1 ; ++i){
        printf("'%s'\n", array+(i*n2));
        }
}

/**
 * @brief      Creates requests messages and add it to the request state structure
 *
 * @param      request_state  The request state structure that we will fill in
 */
void create_requests(request_state_t* request_state){
    
    //create usefull jobs
    for (int i = 0; i < MD5_LIST_NROF ; ++i){
        uint128_t md5 = md5_list[i];
        for (int j = 0; j < ALPHABET_NROF_CHAR ; ++j){
            MQ_REQUEST_MESSAGE req = {i,FALSE,ALPHABET_START_CHAR + j, ALPHABET_START_CHAR, ALPHABET_END_CHAR ,md5};
            request_state->requests[i][j] = req;
            }
        }
    //create kill messages
    MQ_REQUEST_MESSAGE kill_req = {0,TRUE,0,0,0,0};
    for (int i = 0; i < NROF_KILL_MESSAGES; ++i) {
        request_state->kill_request[i] = kill_req;
    }
}

/**
 * main function of the farmer
 * @param  argc
 * @param  argv
 * @return 0 if success, 1 if failure
 */
int main (int argc, char * argv[]){
    if (argc != 1){
        fprintf (stderr, "%s: invalid arguments\n", argv[0]);
    }

    ///////////////////
    // creates queue //
    ///////////////////
    
    struct mq_attr mq_request_attr;
    struct mq_attr mq_response_attr;
    char key_found[MD5_LIST_NROF*(MAX_MESSAGE_LENGTH+1)];     //output array to store the keys
    memset(key_found, 0, sizeof(key_found));                  //initialize it to 0
    
    //creates 2 message queues (request and response)
    mqd_t mq_fd_request;
    mqd_t mq_fd_response;

    create_queue(mq_request_name , "mq_request", sizeof(MQ_REQUEST_MESSAGE) ,O_WRONLY,&mq_fd_request,&mq_request_attr);
    create_queue(mq_response_name,"mq_response", sizeof(MQ_RESPONSE_MESSAGE),O_RDONLY,&mq_fd_response,&mq_response_attr);
    request_state_t request_state;                         // create a request_state structure
    memset(&request_state, 0, sizeof(request_state_t));  //initialize it to 0
    

    ///////////////////////////////////////////////////////////////////////////
    // create request and send them and manage the response and close queues //
    ///////////////////////////////////////////////////////////////////////////
    create_requests(&request_state);
    //  * create the child processes
    create_child_processes();
    //  * do the farming
    send_requests(mq_fd_request, &request_state,NROF_WORKERS_REQUIRED);
    manage_responses(mq_fd_response, mq_fd_request, &request_state, key_found);
    //  * clean up the message queues
    close_queue(mq_fd_response,mq_request_name);
    close_queue(mq_fd_request ,mq_response_name);
    //  * print result
    print_keys_found(key_found, MD5_LIST_NROF, MAX_MESSAGE_LENGTH+1);
    return (0);
}
