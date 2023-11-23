
add_executable(test_seq_arr test_seq_array.c ../seq_arr.c  ../../alg/find.c ../../alg/foreach.c)
add_dependencies(tests test_seq_arr)
add_test(NAME test_seq_arr COMMAND my_new_test) # no options required

