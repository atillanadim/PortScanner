cmake_minimum_required(VERSION 3.16)
project(PortScanner LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# No Windows, o Boost instalado pelo MSYS2 fica fora do PATH padrao.
if(WIN32 AND EXISTS "C:/msys64/mingw64")
    list(APPEND CMAKE_PREFIX_PATH "C:/msys64/mingw64")
endif()

find_package(Boost 1.66 REQUIRED CONFIG COMPONENTS program_options)

add_executable(port_scanner main.cpp src/PortScanner.cpp)

target_link_libraries(port_scanner PRIVATE Boost::program_options)

if(WIN32)
    target_link_libraries(port_scanner PRIVATE ws2_32 mswsock)
endif()
