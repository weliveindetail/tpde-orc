cmake_minimum_required(VERSION 3.16)
project(tpde-orc C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(LLVM_DIR "/usr/lib/llvm-20/lib/cmake/llvm")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(LLVM_DIR "/opt/homebrew/opt/llvm@20/lib/cmake/llvm")
else()
  message(FATAL_ERROR "Unsupported OS")
endif()

find_package(LLVM REQUIRED CONFIG)
llvm_map_components_to_libnames(llvm_libs native orcjit)

add_executable(tpde-orc src/main.cpp)
target_link_libraries(tpde-orc PRIVATE ${llvm_libs})
target_include_directories(tpde-orc PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_options(tpde-orc PRIVATE -Wall -Wextra -Wno-unused-parameter)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_link_options(tpde-orc PRIVATE -fuse-ld=lld)
endif()

if(NOT LLVM_ENABLE_RTTI)
  target_compile_options(tpde-orc PRIVATE -fno-rtti)
endif()
