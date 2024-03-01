cmake_minimum_required(VERSION 3.13)
project(wfsync)

file(GLOB SRC "src/*.cpp")

add_library(wfsync SHARED ${SRC})
target_link_libraries(wfsync PRIVATE sdrpp_core)
set_target_properties(wfsync PROPERTIES PREFIX "")

target_include_directories(wfsync PRIVATE "src/" "../../decoder_modules/radio/src")

if (MSVC)
    target_compile_options(wfsync PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(wfsync PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(wfsync PRIVATE -O3 -std=c++17)
endif ()

# Install directives
install(TARGETS wfsync DESTINATION lib/sdrpp/plugins)