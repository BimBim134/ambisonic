cmake_minimum_required(VERSION 3.16)

project(ambisonic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)

add_library(sde_core STATIC
  src/audio.cpp
  src/encoder.cpp
  src/spherical_harmonics.cpp
)
target_include_directories(sde_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_link_libraries(sde_core PUBLIC Threads::Threads)

if(MSVC)
  target_compile_options(sde_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(sde_core PRIVATE -Wall -Wextra)
endif()

add_executable(ambisonic src/main.cpp)
target_link_libraries(ambisonic PRIVATE sde_core)
if(MSVC)
  target_compile_options(ambisonic PRIVATE /W4 /permissive-)
else()
  target_compile_options(ambisonic PRIVATE -Wall -Wextra)
endif()