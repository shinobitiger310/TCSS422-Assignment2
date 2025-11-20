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
int fill = 0; // index of next empty slot in buffer, only accessed by producers
int use = 0; // index of next getable slot in buffer, only accessed by consumers
pthread_cond_t empty = PTHREAD_COND_INITIALIZER; // condition variable that producers wait on when buffer is full
pthread_cond_t full = PTHREAD_COND_INITIALIZER; // condition variable that consumers wait on when buffer is empty
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // mutex that controls access to the buffer

// Helper function to handle consumer termination cleanup
// Wakes other consumers, unlocks mutex, frees m1 if provided, returns stats
void* cleanup_and_exit_consumer(Matrix *m1, ProdConsStats *stats)
{
  pthread_cond_broadcast(&full);  // Wake up other waiting consumers
  pthread_mutex_unlock(&mutex); // Ensure mutex is unlocked
  if (m1 != NULL) {
    FreeMatrix(m1); // Free matrix if provided
  }
  return (void*) stats; // Return stats
}

// Helper function to wait for buffer data and check if work is done
// Waits while buffer is empty, then checks if all matrices consumed
// Returns cleanup_and_exit_consumer result if done, NULL if can continue
// Must be called while holding the mutex
void* wait_for_buffer_or_exit(Matrix *m, ProdConsStats *cons)
{
  // wait while the buffer is empty and the work is not done
  while (get_cnt(prodc) == get_cnt(conc) && get_cnt(conc) < NUMBER_OF_MATRICES)
    pthread_cond_wait(&full, &mutex);

  // After waking, check if we've consumed all matrices
  if (get_cnt(conc) >= NUMBER_OF_MATRICES) {
    return cleanup_and_exit_consumer(m, cons);
  }

  return NULL; // Can continue
}

// Bounded buffer put() get() routines

// put a matrix into the bounded buffer
int put(Matrix * value) 
{
    bigmatrix[fill] = value; // put matrix into buffer
    fill = (fill + 1) % BOUNDED_BUFFER_SIZE; // update fill index
    increment_cnt(prodc); // increment number of produced matrices
    return get_cnt(prodc); // return total number of produced matrices
}

// get a matrix from the bounded buffer
Matrix * get()
{
  Matrix *tmp = bigmatrix[use]; // get matrix from buffer
  use = (use + 1) % BOUNDED_BUFFER_SIZE; // update use index
  increment_cnt(conc); // increment number of consumed matrices
  return tmp; // return matrix
}

// Matrix PRODUCER worker thread
void *prod_worker(void *arg)
{
  // Extract work count from argument
  int *num_to_produce = (int*)arg; // cast argument to int pointer
  int work_count = *num_to_produce; // dereference to get work count
  free(num_to_produce);  // Free the allocated int

  // variable to hold progression stats
  ProdConsStats *prods = malloc(sizeof(ProdConsStats));
  prods->sumtotal = 0; // sumtotal - total of all elements produced or consumed
  prods->matrixtotal = 0; // matrixtotal - total number of matrces produced or consumed
  prods->multtotal = 0; // multtotal - total number of matrices multipled

  Matrix *produced; // variable to hold produced matrix

  int i;
  // Produce matrices work_count times
  for (i = 0; i < work_count; i++) {
    produced = GenMatrixRandom(); // generate random matrix

    prods->sumtotal += SumMatrix(produced); // Sum the matrix before putting it in buffer
    prods->matrixtotal++; // increment produced matrix count

    pthread_mutex_lock(&mutex); // lock the mutex before accessing the buffer
    // wait while buffer is full
    while (get_cnt(prodc) - get_cnt(conc) >= BOUNDED_BUFFER_SIZE) // check if produced matrices - consumed matrices >= buffer size
        pthread_cond_wait(&empty, &mutex); // wait until signaled that buffer has space
    put(produced); // put produced matrix into buffer
    pthread_cond_signal(&full); // signal that buffer has data
    pthread_mutex_unlock(&mutex); // unlock the mutex after accessing the buffer
  }

  return (void*) prods; // return progression stats
}

// Matrix CONSUMER worker thread
void *cons_worker(void *arg)
{
  // variable to hold progression stats
  ProdConsStats *cons = malloc(sizeof(ProdConsStats));
  cons->sumtotal = 0;
  cons->matrixtotal = 0;
  cons->multtotal = 0;
  
  // m1,m2: maxtrices to multiply; m3: result matrix
  Matrix *m1, *m2, *m3;
  
  // Continue until all matrices consumed
  while (1) {
      pthread_mutex_lock(&mutex); // lock the mutex before accessing the buffer

      // Check if we've consumed all matrices
      if (get_cnt(conc) >= NUMBER_OF_MATRICES) {
          // Wakes other consumers, unlocks mutex, returns stats
          return cleanup_and_exit_consumer(NULL, cons);
      }

      // Wait for buffer data or exit if all work is done
      void* result = wait_for_buffer_or_exit(NULL, cons);
      if (result != NULL) {
          return result;
      }

      // If we reach here, buffer must have data
      m1 = get(); // get first matrix
      pthread_cond_signal(&empty); // signal that buffer has space
      pthread_mutex_unlock(&mutex); // unlock the mutex
      
      cons->matrixtotal++; // increment consumed matrix count
      cons->sumtotal += SumMatrix(m1); // sum the matrix
      
      pthread_mutex_lock(&mutex); // lock the mutex before accessing the buffer

      // If we've consumed all matrices, wakes other consumers, unlocks mutex, frees m1, returns stats
      if (get_cnt(conc) >= NUMBER_OF_MATRICES) {
          return cleanup_and_exit_consumer(m1, cons);
      }

      // Wait for buffer data or exit if all work is done
      result = wait_for_buffer_or_exit(m1, cons);
      if (result != NULL) {
          return result;
      }

      // If we reach here, buffer must have data
      m2 = get();
      pthread_cond_signal(&empty); // signal that buffer has space
      pthread_mutex_unlock(&mutex); // unlock the mutex

      cons->matrixtotal++; // increment consumed matrix count
      cons->sumtotal += SumMatrix(m2); // sum the matrix

      m3 = MatrixMultiply(m1, m2); // multiply matrices
      
      while (m3 == NULL) { // while multiplication failed (incompatible sizes)
        FreeMatrix(m2); // free second matrix

        pthread_mutex_lock(&mutex); // lock the mutex before accessing the buffer

        // Check if all possible matrices have been consumed and we need to stop
        if (get_cnt(conc) >= NUMBER_OF_MATRICES) {
          return cleanup_and_exit_consumer(m1, cons);
        }

        // Wait for buffer data or exit if all work is done
        result = wait_for_buffer_or_exit(m1, cons);
        if (result != NULL) {
          return result;
        }

        // If we reach here, buffer must have data
        m2 = get();
        pthread_cond_signal(&empty); // signal that buffer has space
        pthread_mutex_unlock(&mutex); // unlock the mutex

        cons->matrixtotal++; // increase the tracker for total number of matrices consumed by 1
        cons->sumtotal += SumMatrix(m2); // increase the tracker for total sum of all consumed by sum of the matrix
        m3 = MatrixMultiply(m1, m2); // multiply the matrices together, will return NULL if incompatible
      }
      
      // Print the multiplication result
      DisplayMatrix(m1, stdout);
      printf("    X\n");
      DisplayMatrix(m2, stdout);
      printf("    =\n");
      DisplayMatrix(m3, stdout);
      printf("\n");
      
      // Free matrices
      FreeMatrix(m1);
      FreeMatrix(m2);
      FreeMatrix(m3);
      
      // increase total successfully multiplied tracker by 1
      cons->multtotal++;
      
  }
  
  return (void*) cons; // return progression stats
}
