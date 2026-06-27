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

add_executable(test_scanner test_scanner.c)
target_link_libraries(test_scanner PRIVATE salvage_lib)
add_test(NAME test_scanner COMMAND test_scanner)

add_executable(test_fat test_fat.c)
target_link_libraries(test_fat PRIVATE salvage_lib)
add_test(NAME test_fat COMMAND test_fat)

add_executable(test_full test_full.c)
target_link_libraries(test_full PRIVATE salvage_lib)
add_test(NAME test_full COMMAND test_full)
