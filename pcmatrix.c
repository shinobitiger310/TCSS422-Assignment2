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
  if (argc==1)
  {
    BOUNDED_BUFFER_SIZE=MAX;
    NUMBER_OF_MATRICES=LOOPS;
    MATRIX_MODE=DEFAULT_MATRIX_MODE;
    printf("USING DEFAULTS: worker_threads=%d bounded_buffer_size=%d matricies=%d matrix_mode=%d\n",numw,BOUNDED_BUFFER_SIZE,NUMBER_OF_MATRICES,MATRIX_MODE);
  }
  else
  {
    if (argc==2)
    {
      numw=atoi(argv[1]);
      BOUNDED_BUFFER_SIZE=MAX;
      NUMBER_OF_MATRICES=LOOPS;
      MATRIX_MODE=DEFAULT_MATRIX_MODE;
    }
    if (argc==3)
    {
      numw=atoi(argv[1]);
      BOUNDED_BUFFER_SIZE=atoi(argv[2]);
      NUMBER_OF_MATRICES=LOOPS;
      MATRIX_MODE=DEFAULT_MATRIX_MODE;
    }
    if (argc==4)
    {
      numw=atoi(argv[1]);
      BOUNDED_BUFFER_SIZE=atoi(argv[2]);
      NUMBER_OF_MATRICES=atoi(argv[3]);
      MATRIX_MODE=DEFAULT_MATRIX_MODE;
    }
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

  printf("Producing %d matrices in mode %d.\n",NUMBER_OF_MATRICES,MATRIX_MODE);
  printf("Using a shared buffer of size=%d\n", BOUNDED_BUFFER_SIZE);
  printf("With %d producer and consumer thread(s).\n",numw);
  printf("\n");

  
  bigmatrix = (Matrix **) malloc(sizeof(Matrix *) * BOUNDED_BUFFER_SIZE); // allocate bounded buffer matrix array

  prodc = (counter_t *) malloc(sizeof(counter_t)); // allocate counters for produced matrices
  conc = (counter_t *) malloc(sizeof(counter_t)); // allocate counters for consumed matrices
  init_cnt(prodc); // initialize counters for produced matrices
  init_cnt(conc); // initialize counters

  // Allocate arrays for multiple producers and consumers
  pthread_t *producers = (pthread_t *) malloc(sizeof(pthread_t) * numw);
  pthread_t *consumers = (pthread_t *) malloc(sizeof(pthread_t) * numw);

  // Allocate arrays for ProdConsStats structs
  ProdConsStats **prod_stats = (ProdConsStats **) malloc(sizeof(ProdConsStats *) * numw);
  ProdConsStats **cons_stats = (ProdConsStats **) malloc(sizeof(ProdConsStats *) * numw);

  // Calculate work distribution for producers
  int matrices_per_producer = NUMBER_OF_MATRICES / numw; // base number of matrices per producer
  int remainder = NUMBER_OF_MATRICES % numw; // remainder matrices to distribute

  // Create producer threads with specific work counts
  for (int i = 0; i < numw; i++) {
    int *work = (int *) malloc(sizeof(int)); // allocate memory for work count
    *work = matrices_per_producer + (i < remainder ? 1 : 0); // distribute remainder among producers
    pthread_create(&producers[i], NULL, prod_worker, work); // create producer thread
  }

  // Create consumer threads
  for (int i = 0; i < numw; i++) {
    pthread_create(&consumers[i], NULL, cons_worker, NULL); // create consumer thread
  }

  // These are used to aggregate total numbers for main thread output
  int prs = 0; // total #matrices produced
  int cos = 0; // total #matrices consumed
  int prodtot = 0; // total sum of elements for matrices produced
  int constot = 0; // total sum of elements for matrices consumed
  int consmul = 0; // total # multiplications

  // Join producer threads and aggregate their stats
  for (int i = 0; i < numw; i++) {
    pthread_join(producers[i], (void**) &prod_stats[i]);
    prs += prod_stats[i]->matrixtotal;
    prodtot += prod_stats[i]->sumtotal;
  }

  // Join consumer threads and aggregate their stats
  for (int i = 0; i < numw; i++) {
    pthread_join(consumers[i], (void**) &cons_stats[i]);
    cos += cons_stats[i]->matrixtotal;
    constot += cons_stats[i]->sumtotal;
    consmul += cons_stats[i]->multtotal;
  }

  printf("Sum of Matrix elements --> Produced=%d = Consumed=%d\n",prodtot,constot);
  printf("Matrices produced=%d consumed=%d multiplied=%d\n",prs,cos,consmul);

  // Clean up allocated memory
  for (int i = 0; i < numw; i++) {
    free(prod_stats[i]);
    free(cons_stats[i]);
  }
  free(producers);
  free(consumers);
  free(prod_stats);
  free(cons_stats);
  free(bigmatrix);
  free(prodc);
  free(conc);
  return 0;
}
