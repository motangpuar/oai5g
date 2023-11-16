/*
MIT License

Copyright (c) 2022 Mikel Irazabal

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef FIND_ALGORITHM
#define FIND_ALGORITHM 

#include <stdbool.h>
#include "../ds/seq_arr.h"

// Sequencial containers

// Returns the iterator if the predicate (i.e., f) is true. It returs the seq_arr_end() iterator if not found 
void* find_if_arr(seq_arr_t* arr, void* value, bool(*f)(const void* value, const void* it));

// Returns the iterator if the predicate (i.e., f) within the range (i.e., [start_it, end_if) ) is true. It returs the seq_arr_end() iterator if not found 
void* find_if_arr_it(seq_arr_t* arr, void* start_it, void* end_it, void* value , bool(*f)(const void* value, const void* it));

#endif

