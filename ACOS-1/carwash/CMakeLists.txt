cmake_minimum_required(VERSION 3.16)
project(carwash C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # нужен clang-tidy

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# бинарники кладём в bin/, чтобы не искать их внутри build/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

file(GLOB SOURCES CONFIGURE_DEPENDS src/*.c)
file(GLOB HEADERS CONFIGURE_DEPENDS include/*.h)
set(TEST_SOURCES tests/run_tests.c)

add_executable(carwash ${SOURCES})
target_include_directories(carwash PRIVATE include)

add_executable(run_tests ${TEST_SOURCES})

foreach(target carwash run_tests)
    target_compile_definitions(${target} PRIVATE _POSIX_C_SOURCE=200809L)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
endforeach()

# cmake --build build --target check (или ctest --test-dir build)
enable_testing()
add_test(NAME scenarios COMMAND run_tests $<TARGET_FILE:carwash>
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_custom_target(check
    COMMAND run_tests $<TARGET_FILE:carwash>
    DEPENDS carwash run_tests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL)

# clang-format и clang-tidy. В Homebrew llvm не попадает в PATH, поэтому смотрим и туда
set(LLVM_HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin)
find_program(CLANG_FORMAT clang-format HINTS ${LLVM_HINTS})
find_program(CLANG_TIDY clang-tidy HINTS ${LLVM_HINTS})
set(ALL_CODE ${SOURCES} ${HEADERS} ${TEST_SOURCES})

if(CLANG_FORMAT)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${ALL_CODE}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
else()
    add_custom_target(format
        COMMAND ${CMAKE_COMMAND} -E echo "clang-format not found: brew install llvm (or apt install clang-format)"
        COMMAND ${CMAKE_COMMAND} -E false)
endif()

# clang-tidy из Homebrew сам не знает, где на macOS системные заголовки
set(TIDY_EXTRA "")
if(APPLE)
    execute_process(COMMAND xcrun --show-sdk-path
                    OUTPUT_VARIABLE MACOS_SDK OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(TIDY_EXTRA --extra-arg=-isysroot --extra-arg=${MACOS_SDK})
endif()

if(CLANG_TIDY)
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY} -p ${CMAKE_BINARY_DIR} --quiet ${TIDY_EXTRA} ${SOURCES} ${TEST_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
else()
    add_custom_target(tidy
        COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy not found: brew install llvm (or apt install clang-tidy)"
        COMMAND ${CMAKE_COMMAND} -E false)
endif()

# Doxygen: cmake --build build --target docs, открыть build/doxygen/html/index.html
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
else()
    add_custom_target(docs
        COMMAND ${CMAKE_COMMAND} -E echo "doxygen not found: brew install doxygen (or apt install doxygen)"
        COMMAND ${CMAKE_COMMAND} -E false)
endif()
