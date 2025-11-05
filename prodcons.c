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
int fill = 0;
int use = 0;
int count = 0;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t full = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


// Bounded buffer put() get()
int put(Matrix * value)
{
    bigMatrix[fill] = value;
    fill = (fill + 1) % BOUNDED_BUFFER_SIZE;
    count++;
    return count;
}

Matrix * get()
{
  Matrix *tmp = bigMatrix[use];
  use = (use + 1) % BOUNDED_BUFFER_SIZE;
  count --;
  return tmp;
}

// Matrix PRODUCER worker thread
void *prod_worker(void *arg)
{
    int i;
    for (i = 0; i < NUMBER_OF_MATRICES; i++) {
        Matrix *value = GenMatrixRandom();
        pthread_mutex_lock(&mutex);
        while (count == BOUNDED_BUFFER_SIZE)
            pthread_cond_wait(&empty, &mutex);
        put(value);
        pthread_cond_signal(&full);
        pthread_mutex_unlock(&mutex);
    }
  return NULL;
}

// Matrix CONSUMER worker thread
void *cons_worker(void *arg)
{
    int i;
    for (i = 0; i < NUMBER_OF_MATRICES; i++) {
        pthread_mutex_lock(&mutex);
        while (count == 0)
            pthread_cond_wait(&full, &mutex);
        Matrix *tmp = get();
        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&mutex);
    }
  return NULL;
}
