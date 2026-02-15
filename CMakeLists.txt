cmake_minimum_required(VERSION 3.16)
project(GB-Emu-2k26 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)


file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(gbemu ${SOURCES})

target_include_directories(gbemu PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(gbemu PRIVATE ${SDL2_LIBRARIES})

if(WIN32)
    target_link_libraries(gbemu PRIVATE Comdlg32)
endif()
