cmake_minimum_required(VERSION 3.22)
project(FLASH-TB)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

#Compiler settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-pipe -DUSE_SIMD -DENABLE_PREFETCH -march=native -Wfatal-errors -g")
set(CMAKE_CXX_FLAGS_DEBUG "-rdynamic -Werror -Wpedantic -pedantic-errors -Wall -Wextra -Wparentheses -D_GLIBCXX_DEBUG -fno-omit-frame-pointer -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-ffast-math -ftree-vectorize -DNDEBUG -O3")

#Libraries
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)
find_package(OpenMP REQUIRED)

# for the sparsehash-map in Transfer Patterns
include_directories(${PROJECT_SOURCE_DIR}/../ExternalLibs/sparsehash/include)

function(add_runnable name)
    add_executable(${ARGV})
    target_link_libraries(${name} PUBLIC OpenMP::OpenMP_CXX)
    target_include_directories(${name} PUBLIC ${PROJECT_SOURCE_DIR}/../ExternalLibs/sparsehash/include)
endfunction()

set(RUNNABLES FLASHTB Network TP ULTRA)
foreach(f ${RUNNABLES})
    add_runnable(${f} Runnables/${f}.cpp)
endforeach()

add_custom_target(All)
add_dependencies(All ${RUNNABLES})
