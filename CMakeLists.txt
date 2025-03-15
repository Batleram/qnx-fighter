cmake_minimum_required(VERSION 3.11) # FetchContent is available in 3.11+

# SET(CMAKE_SYSTEM_NAME QNX)
# SET(CMAKE_C_COMPILER   /home/me/qnx800/host/linux/x86_64/usr/bin/ntoaarch64-gcc)
# SET(CMAKE_CXX_COMPILER /home/me/qnx800/host/linux/x86_64/usr/bin/ntoaarch64-g++)
# SET(CMAKE_AR /home/me/qnx800/host/linux/x86_64/usr/bin/ntoaarch64-ar CACHE PATH "QNX AR Program" )
# SET(ENV{QNX_HOST} /home/me/qnx800/host/linux/x86_64/)
# SET(ENV{QNX_TARGET} /home/me/qnx800/target/qnx)
#
# SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
# SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

SET(PLATFORM Desktop)
SET(GLFW_BUILD_X11 FALSE)
SET(GLFW_BUILD_WAYLAND TRUE)

project(
    qnx_fighter
    VERSION 1.0.0
    LANGUAGES CXX
)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_definitions(__EXT_POSIX1_199309=TRUE)
add_compile_definitions(_XOPEN_SOURCE=700)
set(CMAKE_C_FLAGS "-D_QNX_SOURCE -D__EXT_UNIX_MISC -D__EXT_QNX -DMA_USE_STDINT")
# add_compile_definitions(__EXT_QNX)

# Dependencies
add_subdirectory(raylib)

# Our Project

add_executable(${PROJECT_NAME} main.cpp)
#set(raylib_VERBOSE 1)
target_link_libraries(${PROJECT_NAME} raylib)

