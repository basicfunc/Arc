cmake_minimum_required(VERSION 3.10)
project(ARCExample)

# Add the include directory for ARC
include_directories(include)

# Build the example executable
add_executable(example main.cpp)

# Set C++ standard
# set_property(TARGET example PROPERTY CXX_STANDARD 11)
