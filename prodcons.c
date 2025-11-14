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
#include "counter.h"
#include "matrix.h"
#include "pcmatrix.h"
#include "prodcons.h"


// Define Locks, Condition variables, and so on here
int fill = 0; // only accessed by producers
int use = 0; // only accessed by consumers
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t full = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Bounded buffer put() get() routines

int put(Matrix * value)
{
    bigmatrix[fill] = value;
    fill = (fill + 1) % BOUNDED_BUFFER_SIZE;
    increment_cnt(prodc);
    return 1;
}

Matrix * get()
{
  Matrix *tmp = bigmatrix[use];
  use = (use + 1) % BOUNDED_BUFFER_SIZE;
  increment_cnt(conc);
  return tmp;
}

// Matrix PRODUCER worker thread
void *prod_worker(void *arg)
{
  // Extract work count from argument
  int *num_to_produce = (int*)arg;
  int work_count = *num_to_produce;
  free(num_to_produce);  // Free the allocated int

  int i;
  ProdConsStats *prods = malloc(sizeof(ProdConsStats));
  prods->sumtotal = 0;
  prods->matrixtotal = 0;
  prods->multtotal = 0;

  Matrix *produced;

  for (i = 0; i < work_count; i++) {
    produced = GenMatrixRandom();

    // Sum the matrix BEFORE putting it in buffer (while we still own it)
    prods->sumtotal += SumMatrix(produced);
    prods->matrixtotal++;

    pthread_mutex_lock(&mutex);
    // wait while buffer is full
    while (get_cnt(prodc) - get_cnt(conc) >= BOUNDED_BUFFER_SIZE)
        pthread_cond_wait(&empty, &mutex);
    put(produced);
    pthread_cond_signal(&full);
    pthread_mutex_unlock(&mutex);
  }

  return (void*) prods;
}

// Matrix CONSUMER worker thread
void *cons_worker(void *arg)
{
  ProdConsStats *cons = malloc(sizeof(ProdConsStats));
  cons->sumtotal = 0;
  cons->matrixtotal = 0;
  cons->multtotal = 0;
  
  Matrix *m1, *m2, *m3;
  
  // Continue until all matrices consumed
  while (1) {
      pthread_mutex_lock(&mutex);

      // Check if we've consumed all matrices
      if (get_cnt(conc) >= NUMBER_OF_MATRICES) {
          pthread_cond_broadcast(&full);  // Wake up other waiting consumers
          pthread_mutex_unlock(&mutex);
          break;
      }

      // wait while the buffer is empty AND not done
      while (get_cnt(prodc) == get_cnt(conc) && get_cnt(conc) < NUMBER_OF_MATRICES)
          pthread_cond_wait(&full, &mutex);

      // After waking, check if we're done (termination condition)
      if (get_cnt(conc) >= NUMBER_OF_MATRICES) {
          pthread_cond_broadcast(&full);  // Wake up other waiting consumers
          pthread_mutex_unlock(&mutex);
          break;
      }

      // If we reach here, buffer must have data (while loop exited, not done, holding mutex)
      m1 = get();
      pthread_cond_signal(&empty);
      pthread_mutex_unlock(&mutex);
      
      cons->matrixtotal++;
      cons->sumtotal += SumMatrix(m1);
      
      pthread_mutex_lock(&mutex);

      // Check again before getting m2
      if (get_cnt(conc) >= NUMBER_OF_MATRICES) {
          pthread_cond_broadcast(&full);  // Wake up other waiting consumers
          pthread_mutex_unlock(&mutex);
          FreeMatrix(m1);
          break;
      }

      // wait while the buffer is empty AND not done
      while (get_cnt(prodc) == get_cnt(conc) && get_cnt(conc) < NUMBER_OF_MATRICES)
          pthread_cond_wait(&full, &mutex);

      // After waking, check if we're done (termination condition)
      if (get_cnt(conc) >= NUMBER_OF_MATRICES) {
          pthread_cond_broadcast(&full);  // Wake up other waiting consumers
          pthread_mutex_unlock(&mutex);
          FreeMatrix(m1);
          break;
      }

      // If we reach here, buffer must have data (while loop exited, not done, holding mutex)
      m2 = get();
      pthread_cond_signal(&empty);
      pthread_mutex_unlock(&mutex);
      
      cons->matrixtotal++;
      cons->sumtotal += SumMatrix(m2);
      
      m3 = MatrixMultiply(m1, m2);
      
      while (m3 == NULL) {
        FreeMatrix(m2);

        pthread_mutex_lock(&mutex);

        // Check if all matrices have been produced and we need to stop
        if (get_cnt(conc) >= NUMBER_OF_MATRICES) {
          pthread_cond_broadcast(&full);  // Wake up other waiting consumers
          pthread_mutex_unlock(&mutex);
          FreeMatrix(m1);
          return (void*) cons;
        }

        // wait while the buffer is empty AND not done
        while (get_cnt(prodc) == get_cnt(conc) && get_cnt(conc) < NUMBER_OF_MATRICES)
          pthread_cond_wait(&full, &mutex);

        // After waking, check if we're done (termination condition)
        if (get_cnt(conc) >= NUMBER_OF_MATRICES) {
          pthread_cond_broadcast(&full);  // Wake up other waiting consumers
          pthread_mutex_unlock(&mutex);
          FreeMatrix(m1);
          return (void*) cons;
        }

        // If we reach here, buffer must have data (while loop exited, not done, holding mutex)
        m2 = get();
        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&mutex);

        cons->matrixtotal++;
        cons->sumtotal += SumMatrix(m2);
        m3 = MatrixMultiply(m1, m2);
      }
      
      DisplayMatrix(m1, stdout);
      printf("    X\n");
      DisplayMatrix(m2, stdout);
      printf("    =\n");
      DisplayMatrix(m3, stdout);
      printf("\n");
      
      FreeMatrix(m1);
      FreeMatrix(m2);
      FreeMatrix(m3);
      
      cons->multtotal++;
      
  }
  
  return (void*) cons;
}
