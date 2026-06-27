cmake_minimum_required(VERSION 3.14)
project(Salvage VERSION 0.2.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Platform detection
if(WIN32)
    add_definitions(-DPLATFORM_WINDOWS -D_CRT_SECURE_NO_WARNINGS)
elseif(UNIX AND NOT APPLE)
    add_definitions(-DPLATFORM_LINUX)
elseif(APPLE)
    add_definitions(-DPLATFORM_MACOS)
endif()

# Build options
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# Warning flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
elseif(MSVC)
    add_compile_options(/utf-8)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

# Source files
set(UTILS_SOURCES
    src/utils/log.c
    src/utils/str.c
    src/utils/time.c
    src/utils/progress.c
)

set(DEVICE_SOURCES
    src/device/device.c
)

if(WIN32)
    list(APPEND DEVICE_SOURCES src/device/device_win.c)
else()
    list(APPEND DEVICE_SOURCES src/device/device_linux.c)
endif()

set(PARTITION_SOURCES
    src/partition/mbr.c
    src/partition/gpt.c
    src/partition/detect.c
)

set(NTFS_SOURCES
    src/fs/ntfs/boot.c
    src/fs/ntfs/mft.c
    src/fs/ntfs/attribute.c
    src/fs/ntfs/data_run.c
)

set(FAT_SOURCES
    src/fs/fat/fat_boot.c
    src/fs/fat/fat_dir.c
    src/fs/fat/fat_data.c
)

set(EXFAT_SOURCES
    src/fs/exfat/exfat_boot.c
    src/fs/exfat/exfat_dir.c
    src/fs/exfat/exfat_data.c
)

set(SIGNATURE_SOURCES
    src/signature/database.c
    src/signature/matcher.c
)

set(CORE_SOURCES
    src/core/scanner.c
    src/core/result.c
    src/core/recover.c
)

# Static library
add_library(salvage_lib STATIC
    ${UTILS_SOURCES}
    ${DEVICE_SOURCES}
    ${PARTITION_SOURCES}
    ${NTFS_SOURCES}
    ${FAT_SOURCES}
    ${EXFAT_SOURCES}
    ${SIGNATURE_SOURCES}
    ${CORE_SOURCES}
)
target_include_directories(salvage_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(salvage_lib PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Main executable
add_executable(salvage
    src/cli/main.c
    src/cli/cmd_list.c
    src/cli/cmd_scan.c
    src/cli/cmd_recover.c
)
target_link_libraries(salvage PRIVATE salvage_lib)

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
