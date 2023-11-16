#include "seq_arr.h"
#include "../alg/find.h"
#include <assert.h>

/*
  Example to show seq_arr_t capabilities and usage 
  To compile: gcc seq_arr.c test_seq_array.c ../alg/find.c
*/


static
bool eq_int(const void* value, const void* it)
{
  const int* v = (const int *)value;
  const int* i = (const int*)it;
  return *v == *i;
}


int main()
{
  seq_arr_t arr = {0};
  seq_arr_init(&arr, sizeof(int));

  // Insert data and expand
  for(int i = 0; i < 100; ++i)
    seq_arr_push_back(&arr, &i, sizeof(int));

  // Check inserted data
  assert(seq_arr_size(&arr) == 100);
  assert(*(int*)seq_arr_front(&arr) == 0);
  assert(*(int*)seq_arr_at(&arr, 25) == 25);

  // Find element in the array
  int value = 50;
  void* it = find_if_arr(&arr, &value, eq_int);
  //Check
  assert(*(int*)it == 50);
  assert(seq_arr_dist(&arr, seq_arr_front(&arr), it) == 50);

  // Erase found element in the array
  seq_arr_erase(&arr, it);
  // Check
  assert(seq_arr_size(&arr) == 99);
  assert(*(int*)seq_arr_at(&arr, 50) == 51);

  // Erase range and force shrink
  seq_arr_erase_it(&arr, seq_arr_front(&arr), seq_arr_at(&arr, 90) ,NULL);
  assert(seq_arr_size(&arr) == 9);
  assert(*(int*)seq_arr_front(&arr) == 91);

  // Free data structure
  seq_arr_free(&arr, NULL);

  return EXIT_SUCCESS;
}

