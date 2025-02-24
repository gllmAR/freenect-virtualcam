cmake_minimum_required(VERSION 3.10)
project(freenectVirtualCamera)

# Use C++11.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Locate libfreenect via pkg-config.
find_package(PkgConfig REQUIRED)
pkg_check_modules(FREENECT REQUIRED libfreenect)

# Include libfreenect headers and add necessary definitions.
include_directories(${FREENECT_INCLUDE_DIRS})
link_directories(${FREENECT_LIBRARY_DIRS})
add_definitions(${FREENECT_CFLAGS_OTHER})

# Define the executable.
add_executable(${PROJECT_NAME} freenectVirtualCamera.cpp)

# Link against libfreenect and pthread.
target_link_libraries(${PROJECT_NAME} ${FREENECT_LIBRARIES} pthread)

# Linux-specific: if needed, link additional libraries.
if(UNIX AND NOT APPLE)
  target_link_libraries(${PROJECT_NAME} usb-1.0)
endif()
