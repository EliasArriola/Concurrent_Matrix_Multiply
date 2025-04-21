/*
 *  pcmatrix module
 *  Primary module providing control flow for the pcMatrix program
 *
 *  Producer consumer bounded buffer program to produce random matrices in parallel
 *  and consume them while searching for valid pairs for matrix multiplication.
 *  Matrix multiplication requires the first matrix column count equal the
 *  second matrix row count.
 *
 *  A matrix is consumed from the bounded buffer.  Then matrices are consumed
 *  from the bounded buffer, ONE AT A TIME, until an eligible matrix for multiplication
 *  is found.
 *
 *  Totals are tracked using the ProdConsStats Struct for each thread separately:
 *  - the total number of matrices multiplied (multtotal from each consumer thread)
 *  - the total number of matrices produced (matrixtotal from each producer thread)
 *  - the total number of matrices consumed (matrixtotal from each consumer thread)
 *  - the sum of all elements of all matrices produced and consumed (sumtotal from each producer and consumer thread)
 *  
 *  Then, these values from each thread are aggregated in main thread for output
 *
 *  Correct programs will produce and consume the same number of matrices, and
 *  report the same sum for all matrix elements produced and consumed.
 *
 *  Each thread produces a total sum of the value of
 *  randomly generated elements.  Producer sum and consumer sum must match.
 *
 *  University of Washington, Tacoma
 *  TCSS 422 - Operating Systems
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <pthread.h>
 #include <assert.h>
 #include <time.h>
 #include "matrix.h"
 #include "counter.h"
 #include "prodcons.h"
 #include "pcmatrix.h"
 
 
 int main (int argc, char * argv[])
 {
   // Process command line arguments
   int numw = NUMWORK;
   // use defaults if no arguments are given
   if (argc==1)
   {
     BOUNDED_BUFFER_SIZE=MAX;
     NUMBER_OF_MATRICES=LOOPS;
     MATRIX_MODE=DEFAULT_MATRIX_MODE;
     printf("USING DEFAULTS: worker_threads=%d bounded_buffer_size=%d matricies=%d matrix_mode=%d\n",numw,BOUNDED_BUFFER_SIZE,NUMBER_OF_MATRICES,MATRIX_MODE);
   }
   else
   {
     //when user puts one number (e.g: ./pcMatrix 2), this will assign that value to numw and default the rest.
     if (argc==2)
     {
       numw=atoi(argv[1]);
       BOUNDED_BUFFER_SIZE=MAX;
       NUMBER_OF_MATRICES=LOOPS;
       MATRIX_MODE=DEFAULT_MATRIX_MODE;
     }
     //when user puts two numbers (e.g: ./pcMatrix 2 2), this will assign the first value to numw and the second to buffer sized - default the rest.
     if (argc==3)
     {
       numw=atoi(argv[1]);
       BOUNDED_BUFFER_SIZE=atoi(argv[2]);
       NUMBER_OF_MATRICES=LOOPS;
       MATRIX_MODE=DEFAULT_MATRIX_MODE;
     }
     /**
       when user puts 3 numbers (e.g: ./pcMatrix 2 200 1000), this will assign numw, the bounded buffer, 
       and the # of matrices to values 1-3, respectively.
       The last will default.
     */
     if (argc==4)
     {
       numw=atoi(argv[1]);
       BOUNDED_BUFFER_SIZE=atoi(argv[2]);
       NUMBER_OF_MATRICES=atoi(argv[3]);
       MATRIX_MODE=DEFAULT_MATRIX_MODE;
     }
     /** 
       when user puts 4 numbers (e.g: ./pcMatrix 2 200 1000 0), this will assign numw, the bounded buffer, 
       # of matrices, and the mode to values 1-4, respectively.
       No defaults will be used.
     */
     if (argc==5)
     {
       numw=atoi(argv[1]);
       BOUNDED_BUFFER_SIZE=atoi(argv[2]);
       NUMBER_OF_MATRICES=atoi(argv[3]);
       MATRIX_MODE=atoi(argv[4]);
     }
     printf("USING: worker_threads=%d bounded_buffer_size=%d matricies=%d matrix_mode=%d\n",numw,BOUNDED_BUFFER_SIZE,NUMBER_OF_MATRICES,MATRIX_MODE);
   }
 
   time_t t;
   // Seed the random number generator with the system time
   srand((unsigned) time(&t));
   //allocate the bounded buffer, labeled as "big matrix"
   bigmatrix = (Matrix **) malloc(sizeof(Matrix *) * BOUNDED_BUFFER_SIZE);
   //initialize values (in prodcons.c) - will initialize global counters
   initialize();
 
   /**
       since we are using MULTIPLE producer/consumer threads, we need to create an array.
       set to numw (number of threads being used) then loop and create that many threads.
    */
   pthread_t producers[numw];
   pthread_t consumers[numw];
   for (int i = 0; i < numw; i++) {
     pthread_create(&producers[i], NULL, prod_worker, NULL);
     pthread_create(&consumers[i], NULL, cons_worker, NULL);
   }
  // These are used to aggregate total numbers for main thread output
  int prs = 0; // total #matrices produced
  int cos = 0; // total #matrices consumed
  int prodtot = 0; // total sum of elements for matrices produced
  int constot = 0; // total sum of elements for matrices consumed
  int consmul = 0; // total # multiplications
  
  printf("Producing %d matrices in mode %d.\n",NUMBER_OF_MATRICES,MATRIX_MODE);
  printf("Using a shared buffer of size=%d\n", BOUNDED_BUFFER_SIZE);
  printf("With %d producer and consumer thread(s). \n", numw);
  printf("\n");

  // Join the threads and add stats from all threads
  for (int i = 0; i < numw; i++) {
    void *pstats;
    void *cstats;
    
    pthread_join(producers[i], &pstats);
    ProdConsStats *prod_stats = pstats;
    prodtot += prod_stats->sumtotal;
    prs += prod_stats->matrixtotal;
    free(prod_stats);  // Free stats
    
    pthread_join(consumers[i], &cstats);
    ProdConsStats *cons_stats = cstats;
    constot += cons_stats->sumtotal;
    cos += cons_stats->matrixtotal;
    consmul += cons_stats->multtotal;
    free(cons_stats);  // Free stats
  }
 
   // Output the results
   printf("Sum of Matrix elements --> Produced=%d = Consumed=%d\n", prodtot, constot);
   printf("Matrices produced=%d consumed=%d multiplied=%d\n", prs, cos, consmul);
   free(bigmatrix);
   return 0;
 }