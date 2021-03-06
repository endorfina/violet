cmake_minimum_required(VERSION 3.5)

project(violet)

#set(CMAKE_VERBOSE_MAKEFILE ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

if(DEFINED OPENSSL)
    message(STATUS "OPENSSL set to ${OPENSSL}")
else()
    set(OPENSSL TRUE)
    message(STATUS "OPENSSL undefined. Default value is ${OPENSSL}")
endif()

if(OPENSSL)
    add_definitions(-DVIOLET_SOCKET_USE_OPENSSL)
endif()
add_definitions(-DVIOLET_NO_COMPILE_HTTP)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../out)

if (${CMAKE_BUILD_TYPE} STREQUAL Debug)
    add_definitions(-DDEBUG)
    set(CMAKE_CXX_FLAGS "-O0")
    set(CMAKE_DEBUG_POSTFIX d)
else()
    set(CMAKE_CXX_FLAGS "-O3")
endif()

set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -stdlib=libc++ -ffast-math -Wall -Wno-missing-braces -Wno-switch -Wno-overloaded-virtual -Wno-unused-variable  -Wno-conversion-null -Wno-format-security -Wno-parentheses -Wno-sign-compare -Wno-dangling-else")
#set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -pthread")
set(CMAKE_EXE_LINKER_FLAGS "-pthread")

include_directories(${PROJECT_SOURCE_DIR}/echo)
add_library(echo STATIC
            echo/hash.cpp
            echo/tcp.cpp)

add_executable(violet
            pch.h
            main.cpp
            app_lifetime.cpp
            bluescript.cpp
            protocol.cpp
            ht.cpp
            captcha_image_generator.cpp
            blog.cpp
            lodepng.cpp)

set(LIBS "z")
if(OPENSSL)
    set(LIBS ${LIBS} "ssl" "crypto")
endif()

# add lib dependencies
target_link_libraries(violet
                      echo
                      ${LIBS})