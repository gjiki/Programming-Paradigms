#include "hashset.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

void HashSetNew(hashset * h, int elemSize, int numBuckets,
		HashSetHashFunction hashfn, HashSetCompareFunction comparefn, HashSetFreeFunction freefn)
{
	// Check all assert conditions
	assert (elemSize > 0);
	assert (numBuckets > 0);
	assert (hashfn != NULL);
	assert (comparefn != NULL);
	// Asserts checked

	// Initalize stats of numbers and sizes
	h->log_len = 0;
	h->buckets_num = numBuckets;
	h->bucket_size = sizeof(vector);

	// Initialize functions
	h->hashFn = hashfn;
	h->cmpFn = comparefn;
	h->freeFn = freefn;

	h->data = malloc(h->buckets_num * sizeof(vector));
	assert(h->data != NULL);

	// Create vectors
	for (int i = 0; i < h->buckets_num; i++)
	{
		vector * create_vec = (vector *)((char *)h->data + i * sizeof(vector)); 
		VectorNew(create_vec, elemSize, freefn, 4); 
	}
}

void HashSetDispose(hashset * h)
{
	// Delete all the vectors of hashset data
	assert(h->data != NULL);

	for (int i = 0; i < h->buckets_num; i++)
	{
		vector * vec = (vector *)((char *)h->data + i * h->bucket_size);
		VectorDispose(vec);
	}
	free(h->data);
}

int HashSetCount(const hashset * h)
{
	// Return logical size of hashset
	assert(h->data != NULL);
	return h->log_len;
}

void HashSetMap(hashset * h, HashSetMapFunction mapfn, void * auxData)
{
	// Map all the vectors of hashset
	assert(h->data != NULL);
	for (int i = 0; i < h->buckets_num; i++)
	{
		vector * vec = (vector *)((char *)h->data + i * h->bucket_size); 
		VectorMap(vec, mapfn, auxData); 
	}
}

const int kNotFound = -1;
void HashSetEnter(hashset * h, const void * elemAddr)
{
	// Check all assert conditions
	assert(h->data != NULL);
	assert(elemAddr != NULL); 

	int bucket_pos = h->hashFn(elemAddr, h->buckets_num);
	assert(bucket_pos >= 0);
	assert(bucket_pos < h->buckets_num);
	// Asserts checked
	
	// Find vector in the hashset
	vector * vec = (vector *)((char *)h->data + bucket_pos * h->bucket_size);

	// Find element in the vector
	int pos = VectorSearch(vec, elemAddr, h->cmpFn, 0, false);
	
	/* If such element doesn't exist, append it to the vector
		and increase logical length of hashet.
	   If such element exists in the vector, replace it on the position POS. */
	if (pos == kNotFound)
	{
		VectorAppend(vec, elemAddr);
	  	h->log_len++;
	} else VectorReplace(vec, elemAddr, pos);
}

void * HashSetLookup(const hashset * h, const void * elemAddr)
{
	// Check all assert conditions
	assert(h->data != NULL);
	assert(elemAddr != NULL);

	int bucket_pos = h->hashFn(elemAddr, h->buckets_num);
	assert(bucket_pos >= 0);
	assert(bucket_pos < h->buckets_num);
	// Asserts checked

	// Find vector in the hashset
	vector * vec = (vector *)((char *)h->data + bucket_pos * h->bucket_size);

	// Find element in the vector
	int pos = VectorSearch(vec, elemAddr, h->cmpFn, 0, false);
  
	/* If such element doesn't exist, return NULL,
		else return pointer of the element of the POS index in the vector */
	if (pos == kNotFound) return NULL;
	return (void *)VectorNth(vec, pos);
}
