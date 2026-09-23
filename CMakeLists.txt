cmake_minimum_required(VERSION 3.16)
project(Lacooda64 LANGUAGES CXX)

# Header-only usage requirements propagate to every linked consumer.
add_library(Lacooda64 INTERFACE)
add_library(Lacooda64::Lacooda64 ALIAS Lacooda64)
target_compile_features(Lacooda64 INTERFACE cxx_std_20)
target_include_directories(Lacooda64 INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
