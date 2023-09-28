project(3DS)

set(CMAKE_MINIMUM_REQUIRED_VERSION 3.16.3)

set(CXX_STANDARD c++20)

set(SOURCES src/main.cpp
            src/app/Application.cpp
			      src/System.cpp
            src/memory/Bus.cpp
            src/arm/armgeneric.cpp
            src/arm/thumbgeneric.cpp
            src/arm/arm11.cpp
            src/arm/arm9.cpp
            src/arm/cp15.cpp
            src/arm/mpcore_pmr.cpp
            src/dma/cdma.cpp
            src/dma/ndma.cpp
            src/i2c/i2c.cpp
            src/pxi/pxi.cpp
            src/timers/arm9_timers.cpp)

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(3ds ${SOURCES})
set(TARGET_NAME 3ds)

find_package(SDL2 REQUIRED)
include_directories(3ds ${SDL2_INCLUDE_DIRS})

target_link_libraries(3ds ${SDL2_LIBRARIES})

if(MSVC)
  target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX)
else()
  target_compile_options(${TARGET_NAME} PRIVATE -pg -O3 -mincoming-stack-boundary=3 -std=c++20)
  target_link_options(${TARGET_NAME} PRIVATE -pg)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
