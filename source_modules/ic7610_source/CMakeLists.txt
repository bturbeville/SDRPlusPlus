cmake_minimum_required(VERSION 3.13)
project(ic7610_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

if (MSVC)
    target_include_directories(ic7610_source PRIVATE "./src" "FTD3XXLibrary/")
    target_link_directories(ic7610_source PRIVATE "./src" "FTD3XXLibrary/x64/")
    target_link_libraries(ic7610_source PRIVATE FTD3xx)
else (MSVC)
    target_include_directories(ic7610_source PRIVATE "./src" "./FTD3XXLinux/")
    target_link_directories(ic7610_source PRIVATE "./src" "./FTD3XXLinux/")
    target_link_libraries(ic7610_source PRIVATE "libftd3xx-static.a")
endif ()

