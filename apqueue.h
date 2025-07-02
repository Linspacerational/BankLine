#ifndef PRIORITYQUEUE
#define PRIORITYQUEUE

#include <stdio.h>
#include <stdlib.h>

/* maximum size of the priority queue array*/
#define MaxPQSize 50

struct pQueue
{
	/* priority queue array and count*/
	int count;
	DataType pqlist[MaxPQSize];
};
typedef struct pQueue PQueue;

void InitPQueue (PQueue *pq);
/* priority queue modification operations*/
void PQInsert(PQueue *pq, DataType item);
DataType PQDelete(PQueue *pq);
void ClearPQ(PQueue *pq);
/* priority queue test function*/
int PQEmpty(PQueue *pq);
int PQFull(PQueue *pq);
int PQLength(PQueue *pq);

/* initialize priority queue count*/
void InitPQueue(PQueue *pq)
{
	pq->count = 0;
}

/* insert item into the priority queue*/
void PQInsert (PQueue *pq, DataType item)
{
	/* if all elements of pqlist are used, terminate the program*/
	if (pq->count == MaxPQSize)
	{
		printf("Priority queue overflow!\n");
		exit(1);
	}
	/* place item at the rear of the list and increment count*/
	pq->pqlist[pq->count] = item;
	pq->count++;
}

/* delete an element from the priority queue and return its value*/
DataType PQDelete(PQueue *pq)
{
	DataType min;
	int i, minindex = 0;

	if (pq->count > 0)
	{
		/* find the minimum value and its index in pqlist*/
		min = pq->pqlist[0];    /* assume pqlist[0] is the minimum*/

		/* visit remaining elments, updating minimum and index*/
		for (i = 1; i < pq->count; i++)
			if (CompareEvent(&pq->pqlist[i], &min))
			{
				/* new minimum is pqlist[i]. new minindex is i*/
				min = pq->pqlist[i];
				minindex = i;
			}
		/* move rear element to minindex and decrement count*/
		pq->pqlist[minindex] = pq->pqlist[pq->count-1];
		pq->count--;
	}
	/* qlist is empty, terminate the program*/
	else
	{
		printf("Deleting from an empty priority queue!\n");
		exit(1);
	}
	/* return minimum value*/
	return min;
}

/* return number of list elements*/
int PQLength(PQueue *pq)
{
	return pq->count;
}

/* test for an empty priority queue*/
int PQEmpty(PQueue *pq)
{
	return pq->count == 0;
}

/* test for a full priority queue*/
int PQFull(PQueue *pq)
{
	return pq->count == MaxPQSize; 
}

/* clear the priority queue by resetting count to 0*/
void ClearPQ(PQueue *pq)
{
	pq->count = 0;
}

#endif  /* PRIORITYQUEUE */