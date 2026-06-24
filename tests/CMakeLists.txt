cmake_minimum_required(VERSION 3.14)

# Simple test using main() - no external framework needed

add_executable(test_endian test_endian.c)
target_link_libraries(test_endian PRIVATE salvage_lib)
add_test(NAME test_endian COMMAND test_endian)

add_executable(test_partition test_partition.c)
target_link_libraries(test_partition PRIVATE salvage_lib)
add_test(NAME test_partition COMMAND test_partition)

add_executable(test_signature test_signature.c)
target_link_libraries(test_signature PRIVATE salvage_lib)
add_test(NAME test_signature COMMAND test_signature)
