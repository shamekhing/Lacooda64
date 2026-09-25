add_library(Lacooda64Assembly STATIC Assembler.cpp Disassembler.cpp Lowering.cpp)
add_library(Lacooda64::Assembly ALIAS Lacooda64Assembly)
target_link_libraries(Lacooda64Assembly PUBLIC Lacooda64::Core)
