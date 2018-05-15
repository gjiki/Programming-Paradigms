#include "vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void VectorNew(vector * v, int elemSize, VectorFreeFunction freeFn, int initialAllocation)
{
	// Check all assert conditions
	assert(v != NULL);
	assert(elemSize > 0);
	assert(initialAllocation >= 0);
	// Asserts checked

	if (initialAllocation == 0) initialAllocation = 4;

	// Initalize values of the vector struct
	v->log_len = 0;
	v->alloc_len = initialAllocation;
	v->freeFn = freeFn;

	v->elem_size = elemSize;
	v->elems = malloc(v->elem_size * v->alloc_len);
	assert(v->elems != NULL);
}

void VectorDispose(vector * v)
{
	// Check all assert conditions
	assert(v != NULL);
	assert(v->elems != NULL);
	// Asserts checked

	// Delete vector
	if (v->freeFn != NULL)
	{
		for (int i = 0; i < v->log_len; i++)
		{
			v->freeFn((char *)v->elems + i * v->elem_size);
		}
	}
	free(v->elems);
}

int VectorLength(const vector * v)
{
	// Check all assert conditions
	assert(v != NULL);
	assert(v->elems != NULL);
	// Asserts checked

	// Return log_len of vector
	return v->log_len;
}

void * VectorNth(const vector * v, int position)
{
	// Check all assert conditions
	assert(v != NULL);
	assert(v->elems != NULL);
	assert(position >= 0);
	assert(position < v->log_len);
	// Asserts checked
	
	// Initialize and return pointer to the variable of POSITION index
	void * ptr = (char *)v->elems + position * v->elem_size;
	return ptr;
}

void VectorReplace(vector * v, const void * elemAddr, int position)
{
	// Check all assert conditions
	assert(v != NULL);
	assert(v->elems != NULL);
	assert(position >= 0);
	assert(position < v->log_len);
	// Asserts checked

	// Copy value of the elemAddr, free address to the position address
	void * replaceable_addr = (char *)v->elems + position * v->elem_size;
	if (v->freeFn != NULL) v->freeFn(replaceable_addr);
	memcpy(replaceable_addr, elemAddr, v->elem_size);
}

void VectorInsert(vector * v, const void * elemAddr, int position)
{
	// Check all assert conditions
	assert(v != NULL);
	assert(v->elems != NULL);
	assert(position >= 0);
	assert(position <= v->log_len);
	// Asserts checked

	// If there is no space for another variable, grow vector
	if (v->log_len == v->alloc_len) VectorGrow(v);

	// Find out insert position
	char * insert_pos_ptr = (char *)v->elems + position * v->elem_size;
	
	/* If insert position isn't the last one,
		then move all elements after position of insert elemen. */
	if (position != v->log_len)
	{
		char * dest_ptr = insert_pos_ptr + v->elem_size;
		char * end_ptr = (char *)v->elems + v->log_len * v->elem_size;

		int move_bytes_num = end_ptr - insert_pos_ptr;
		memmove(dest_ptr, insert_pos_ptr, move_bytes_num);
	}

	// Copy element in the vector and increase logical length
	memcpy(insert_pos_ptr, elemAddr, v->elem_size);
	v->log_len++;
}

void VectorAppend(vector * v, const void * elemAddr)
{
	// Append is equal to inserting element to the position - v->log_len
	VectorInsert(v, elemAddr, v->log_len);
}

void VectorDelete(vector * v, int position)
{
	// Check all assert conditions
	assert(v != NULL);
	assert(v->elems != NULL);
	assert(position >= 0);
	assert(position < v->log_len);
	// Asserts checked

	if (v->freeFn != NULL)
	{
		void * delete_ptr = (char *)v->elems + position * v->elem_size;
		v->freeFn(delete_ptr);
	}

	// Change positions of the elements of -1 after the position of deleted variable
	for (int i = position; i < v->log_len; i++)
	{
		void * dest_ptr = (char *)v->elems + i * v->elem_size; 
		void * src_ptr = dest_ptr + sizeof(char) * v->elem_size; 
		memcpy(dest_ptr, src_ptr, v->elem_size); 
	}
	v->log_len--;
}

void VectorSort(vector * v, VectorCompareFunction compare)
{
	// Check all assert conditions
	assert(v != NULL);
	assert(v->elems != NULL);
	assert(compare != NULL);
	// Asserts checked

	// Sort elements using compare function
	qsort(v->elems, v->log_len, v->elem_size, compare);
}

void VectorMap(vector * v, VectorMapFunction mapFn, void * auxData)
{
	// Check all assert conditions
	assert(v != NULL);
	assert(v->elems != NULL);
	assert(mapFn != NULL);
	// Asserts checked

	// Map vector
	for (int i = 0; i < v->log_len; i++)
	{
		void * get_var = VectorNth(v, i);
		mapFn(get_var, auxData);
	}
}

static const int kNotFound = -1;
int VectorSearch(const vector * v, const void * key, VectorCompareFunction searchFn, int startIndex, bool isSorted)
{
	// Check all assert conditions
	assert(v != NULL);
	assert(v->elems != NULL);
	assert(searchFn != NULL);
	assert(key != NULL);

	assert(startIndex >= 0);
	assert(startIndex <= v->log_len);
	// Asserts checked

	if (startIndex == v->log_len) return kNotFound;
	
	// Initialize variables for bsearch and lfind functions
	char * pos_ptr;
	void * start_ptr =  (char *)v->elems + startIndex * v->elem_size;
	size_t num = v->log_len - startIndex;
	
	// Find element and save position pointer in pos_ptr
	if (isSorted) pos_ptr =  bsearch(key, start_ptr, num, v->elem_size, searchFn);
		else pos_ptr = lfind(key, start_ptr, &num, v->elem_size, searchFn);
	
	// If there wasn't such element found return -1, else position of that element
	if (pos_ptr == NULL) return kNotFound;
	return (pos_ptr - (char *)v->elems) / v->elem_size;
}

void VectorGrow(vector * v)
{
	// Check all assert conditions
	assert(v != NULL);
	assert(v->elems != NULL);
	// Asserts checked

	// Double value of alloc_len and realloc elements
	v->alloc_len *= 2;
	v->elems = realloc(v->elems, v->alloc_len * v->elem_size);
}