/*
 *  prodcons module
 *  Producer Consumer module
 *
 *  Implements routines for the producer consumer module based on
 *  chapter 30, section 2 of Operating Systems: Three Easy Pieces
 *
 *  University of Washington, Tacoma
 *  TCSS 422 - Operating Systems
 */
// Include only libraries for this module
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#include "counter.h"
#include "matrix.h"
#include "pcmatrix.h"
#include "prodcons.h"

// Define Locks, Condition variables, and so on here
pthread_cond_t empty = PTHREAD_COND_INITIALIZER; 
pthread_cond_t full = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int fill = 0; //producer is filling in the array
int use = 0; //consumer using elements from array
counter_t count; //counter for number of elements in buffer

counter_t produced; //counter for number of produced matrices
counter_t consumed; //counter for number of consumed matrices

// Bounded buffer put() get()
int put(Matrix * value)
{
  bigmatrix[fill] = value;
  fill = (fill+1) % BOUNDED_BUFFER_SIZE;
  increment_cnt(&count); //increment count of matrices in buffer
  increment_cnt(&produced); //Incement count of matrices produced
  return 1;
}

Matrix * get()
{
  Matrix * tmp = bigmatrix[use];
  use = (use+1) % BOUNDED_BUFFER_SIZE;
  decrement_cnt(&count); //decrement count of matrices in buffer
  increment_cnt(&consumed); //increment count of matrices consumed
  return tmp;
}

// Matrix PRODUCER worker thread
void *prod_worker(void *arg)
{
  Matrix * m;
  ProdConsStats *stats =  malloc(sizeof(ProdConsStats));
  stats->sumtotal = 0;
  stats->matrixtotal = 0;
  stats->multtotal = 0;
    //while we havent produced the number of matrices specified by the user, produce
  while (get_cnt(&produced) < NUMBER_OF_MATRICES) {
    m = GenMatrixRandom();  //generate random matrix to put into buffer
    pthread_mutex_lock(&mutex); //lock because we are entering a critical section
    while(get_cnt(&count) == BOUNDED_BUFFER_SIZE && get_cnt(&produced) < NUMBER_OF_MATRICES) {  //if buffer is full and we havent produced all matrices, wait
      pthread_cond_wait(&empty, &mutex);
    }
    if(get_cnt(&produced) < NUMBER_OF_MATRICES) {   //If produced is still less than the number of matrices, put matrix into buffer, update stats
        put(m);
        stats->sumtotal += SumMatrix(m);
        stats->matrixtotal++;
    }
    pthread_cond_signal(&full); //signal to consumer that buffer can be consumed from
    pthread_mutex_unlock(&mutex);   //release lock
  }
  //after producing, signal to all threads that we are done
  pthread_mutex_lock(&mutex);
  pthread_cond_broadcast(&full); 
  pthread_mutex_unlock(&mutex);
  pthread_exit(stats);
}

// Matrix CONSUMER worker thread
void *cons_worker(void *arg) {
    //initialize stats of consumer
    ProdConsStats *stats = malloc(sizeof(ProdConsStats));
    stats->sumtotal = 0;
    stats->matrixtotal = 0;
    stats->multtotal = 0;
    Matrix *m1 = NULL, *m2 = NULL, *m3 = NULL;   //Our matrices for multiplication
    //while our number of consumed matrices is less than the number of matrices, continue consuming
    while(get_cnt(&consumed) < NUMBER_OF_MATRICES) {
        pthread_mutex_lock(&mutex); //lock because we are entering critical section
        // Get first matrix
        while (get_cnt(&count) == 0 && get_cnt(&consumed) < NUMBER_OF_MATRICES) {   //while buffer is empty and we have not fully consumed, wait
            pthread_cond_wait(&full, &mutex);
        }
        if(get_cnt(&consumed) < NUMBER_OF_MATRICES) {   //if we have not fully consumed, get m1 and update stats, signal to producer we consumed
            m1 = get();
            stats->sumtotal += SumMatrix(m1);
            stats->matrixtotal++;
            pthread_cond_signal(&empty);
        }
        if (get_cnt(&count) == 0 && get_cnt(&consumed) == NUMBER_OF_MATRICES) { //if buffer is empty and we consumed the max, jump to exit loop
            /** 
                we were able to research and find this goto method:https://www.geeksforgeeks.org/goto-statement-in-c/ to help with breaking out
                of the loop when we need to exit the thread.
            */
            goto exit_loop;
        }
        while (get_cnt(&count) == 0 && get_cnt(&consumed) < NUMBER_OF_MATRICES) {   //while buffer is empty and we have not finished consuming, wait
            pthread_cond_wait(&full, &mutex);
        }
        //if we have not fully consumed, get m2, update stats and signal to producer we consumed, otherwise
        if(get_cnt(&consumed) < NUMBER_OF_MATRICES) { 
            m2 = get();
            stats->sumtotal += SumMatrix(m2);
            stats->matrixtotal++;
            pthread_cond_signal(&empty);
        } else {
            //see above on goto usage (line 105-108)
            goto exit_loop;
        }
        
        // Try to multiply matrices
        m3 = MatrixMultiply(m1, m2);
        //if m3 is NULL, the multiplcation failed, so we get another m2 until buffer empty or valid multiplication
        while (m3 == NULL) {
            //free m2 and set it to NULL for safety
            FreeMatrix(m2);
            m2 = NULL;
            //if buffer and empty and we have not fully consumed, wait
            while (get_cnt(&count) == 0 && get_cnt(&consumed)<NUMBER_OF_MATRICES) {
                pthread_cond_wait(&full, &mutex);
            }
            
            //if buffer empty and we have fully consumed, jump to exit loop
            if (get_cnt(&count) == 0 && get_cnt(&consumed) == NUMBER_OF_MATRICES) {
                //see above on goto usage (line 105-108)
                goto exit_loop;
            }
            //if we have not fully consumed, get a new m2 and update stats
            if(get_cnt(&consumed) < NUMBER_OF_MATRICES) {
                m2 = get();
                stats->sumtotal += SumMatrix(m2);
                stats->matrixtotal++;
                pthread_cond_signal(&empty);
            } else {
                //see above on goto usage (line 105-108)
                goto exit_loop;
            }
            // try to multiple matrices again
            m3 = MatrixMultiply(m1, m2);
            //increment mult total by 1
        
        }
        stats->multtotal++;
        /**
            When we reached this point, it means that we have successfully multiplied m1 * m2 into our m3 matrix. 
            We display the matrices to follow the same output per the assignment. 
            Free all matrices and assign them null for the next run.        
         */
        DisplayMatrix(m1, stdout);
        printf("\tX\n");
        DisplayMatrix(m2, stdout);
        printf("\t=\n");
        DisplayMatrix(m3, stdout);
        printf("\n");
        FreeMatrix(m3);
        FreeMatrix(m2);
        FreeMatrix(m1);
        m1=NULL;
        m2=NULL;
        m3=NULL;
        //unlock after success 
        pthread_mutex_unlock(&mutex);
    }
//exit_loop goto statements as described from lines 105-108
exit_loop:
    if (m1) { FreeMatrix(m1); m1 = NULL; }
    if (m2) { FreeMatrix(m2); m2 = NULL; }
    if (m3) { FreeMatrix(m3); m3 = NULL; }
    pthread_mutex_unlock(&mutex);
    pthread_mutex_lock(&mutex);
    pthread_cond_broadcast(&empty); 
    pthread_mutex_unlock(&mutex);
    pthread_exit(stats);
}
//initialize global counters
void initialize() {
    init_cnt(&count);
    init_cnt(&produced);
    init_cnt(&consumed);
}