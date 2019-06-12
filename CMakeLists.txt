cmake_minimum_required(VERSION 3.13)
project(dmx_address_translator)

set(CMAKE_CXX_STANDARD 11)

add_executable(dmx_address_translator main.cpp DmxIn.cpp DmxIn.h DmxOut.cpp DmxOut.h)
target_link_libraries(dmx_address_translator -ljson-c -lftdi1 -pthread)