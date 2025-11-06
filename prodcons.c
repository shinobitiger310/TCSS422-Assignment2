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
int count = 0; // shared resource counter (is this safe for many threads?)
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t full = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Bounded buffer put() get() routines
int put(Matrix * value)
{
    bigmatrix[fill] = value;
    fill = (fill + 1) % BOUNDED_BUFFER_SIZE;
    count++;
    return count;
}

Matrix * get()
{
  Matrix *tmp = bigmatrix[use];
  use = (use + 1) % BOUNDED_BUFFER_SIZE;
  count --;
  return tmp;
}

// Matrix PRODUCER worker thread
void *prod_worker(void *arg)
{
    int i;
    ProdConsStats *stats = (ProdConsStats *) arg;
    Matrix *current;
    // implement the stats collection here
    for (i = 0; i < NUMBER_OF_MATRICES; i++) {
        // We need to check matrix_mode and produce accordingly, not just random
        Matrix *value = GenMatrixRandom();
        pthread_mutex_lock(&mutex);
        while (count == BOUNDED_BUFFER_SIZE)
            pthread_cond_wait(&empty, &mutex);
        put(value);
        pthread_cond_signal(&full);
        pthread_mutex_unlock(&mutex);
    }
  return (void*) stats;
}

// Matrix CONSUMER worker thread
void *cons_worker(void *arg)
{
    int i;
    ProdConsStats *stats = (ProdConsStats *) arg;
    Matrix *current;
    // implement the stats collection here
    for (i = 0; i < NUMBER_OF_MATRICES; i++) {
        pthread_mutex_lock(&mutex);
        while (count == 0)
            pthread_cond_wait(&full, &mutex);
        Matrix *tmp = get();
        // Process the matrix (here we just free it)
        FreeMatrix(tmp);

        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&mutex);
    }
  return (void*) stats;
}
