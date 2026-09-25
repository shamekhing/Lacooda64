add_library(Lacooda64Runtime STATIC Runtime.cpp References.cpp Scheduler.cpp Modifiers.cpp Replacements.cpp)
add_library(Lacooda64::Runtime ALIAS Lacooda64Runtime)
target_link_libraries(Lacooda64Runtime PUBLIC Lacooda64::Core)
