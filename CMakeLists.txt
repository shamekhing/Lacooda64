cmake_minimum_required(VERSION 3.16)
project(Lacooda64 LANGUAGES CXX)

option(LACOODA64_BUILD_TESTS "Build ISA, runtime, assembly and corpus checks" OFF)
option(LACOODA64_BUILD_TOOLS "Build the assembler CLI" OFF)

# Public headers retain their include paths. Only include/ is exported.
add_library(Lacooda64 STATIC
    src/lacooda/Machine.cpp
    src/lacooda/Validate.cpp
)
add_library(Lacooda64::Lacooda64 ALIAS Lacooda64)
add_library(Lacooda64::Core ALIAS Lacooda64)
target_compile_features(Lacooda64 PUBLIC cxx_std_20)
target_include_directories(Lacooda64 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Runtime and assembly are independent consumers of the core.
add_library(Lacooda64Runtime STATIC
    src/runtime/Collections.cpp
    src/runtime/Frame.cpp
    src/runtime/Modifiers.cpp
    src/runtime/Numeric.cpp
    src/runtime/References.cpp
    src/runtime/Replacements.cpp
    src/runtime/Runtime.cpp
    src/runtime/Scheduler.cpp
)
add_library(Lacooda64::Runtime ALIAS Lacooda64Runtime)
target_link_libraries(Lacooda64Runtime PUBLIC Lacooda64::Core)

add_library(Lacooda64Assembly STATIC
    src/assembly/Assembler.cpp
    src/assembly/Disassembler.cpp
    src/assembly/Lowering.cpp
)
add_library(Lacooda64::Assembly ALIAS Lacooda64Assembly)
target_link_libraries(Lacooda64Assembly PUBLIC Lacooda64::Core)

if(LACOODA64_BUILD_TOOLS)
    add_executable(lacooda-asm tools/lacooda-asm.cpp)
    target_link_libraries(lacooda-asm PRIVATE Lacooda64::Assembly)
endif()

if(LACOODA64_BUILD_TESTS)
    enable_testing()
    function(lacooda_test name source)
        add_executable(lacooda64_${name}_tests ${source})
        target_link_libraries(lacooda64_${name}_tests PRIVATE ${ARGN})
        target_include_directories(lacooda64_${name}_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        add_test(NAME lacooda64_${name} COMMAND lacooda64_${name}_tests)
    endfunction()
    lacooda_test(encoding tests/core/Encoding.cpp Lacooda64::Core)
    lacooda_test(runtime tests/runtime/Execution.cpp Lacooda64::Runtime)
    lacooda_test(assembly tests/assembly/Assembly.cpp Lacooda64::Assembly)
    lacooda_test(effects tests/effects/Effects.cpp Lacooda64::Assembly Lacooda64::Runtime)
    add_executable(lacooda64_corpus_tests tests/effects/Corpus.cpp)
    target_include_directories(lacooda64_corpus_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(lacooda64_corpus_tests PRIVATE Lacooda64::Assembly Lacooda64::Runtime)
    add_test(NAME lacooda64_corpus COMMAND lacooda64_corpus_tests "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(lacooda64_consumer_tests tests/consumer/Main.cpp tests/consumer/Core.cpp)
    target_link_libraries(lacooda64_consumer_tests PRIVATE Lacooda64::Runtime Lacooda64::Assembly)
    add_test(NAME lacooda64_consumer COMMAND lacooda64_consumer_tests)
    find_package(Python3 3.10 COMPONENTS Interpreter REQUIRED)
    add_test(NAME lacooda64_conversion_freshness COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/convert_effects.py" --check)
    add_test(NAME lacooda64_conversion COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/effects/Conversion.py")
endif()
