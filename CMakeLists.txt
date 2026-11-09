cmake_minimum_required(VERSION 3.16)
project(orderbook-cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(ORDERBOOK_BUILD_TESTS "Build unit tests" ON)
option(ORDERBOOK_SANITIZE "Enable AddressSanitizer + UBSan (GCC/Clang)" OFF)

# ---- library ---------------------------------------------------------------
add_library(orderbook STATIC
  src/order.cpp
  src/orderbook.cpp
)
target_include_directories(orderbook PUBLIC include)
target_compile_features(orderbook PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(orderbook PRIVATE /W4)
else()
  target_compile_options(orderbook PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(ORDERBOOK_SANITIZE AND NOT MSVC)
  target_compile_options(orderbook PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(orderbook PUBLIC -fsanitize=address,undefined)
endif()

# ---- demo ------------------------------------------------------------------
add_executable(orderbook_demo src/main.cpp)
target_link_libraries(orderbook_demo PRIVATE orderbook)

# ---- tests -----------------------------------------------------------------
if(ORDERBOOK_BUILD_TESTS)
  enable_testing()
  add_executable(orderbook_tests tests/orderbook_tests.cpp)
  target_link_libraries(orderbook_tests PRIVATE orderbook)
  add_test(NAME orderbook_tests COMMAND orderbook_tests)
endif()
