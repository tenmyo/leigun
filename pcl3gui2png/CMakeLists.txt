CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

PROJECT(pcl3gui2png C)
#SET(CMAKE_C_COMPILER /usr/bin/gcc)

# ENABLE WARNINGS
ADD_COMPILE_OPTIONS(
  "$<$<C_COMPILER_ID:Clang>:-Wall;-Weverything>"
  "$<$<C_COMPILER_ID:GNU>:-pedantic;-Wall;-Wextra;-Wcast-align;-Wcast-qual;-Wdisabled-optimization;-Wformat=2;-Winit-self;-Wlogical-op;-Wmissing-declarations;-Wmissing-include-dirs;-Wredundant-decls;-Wshadow;-Wsign-conversion;-Wstrict-overflow=5;-Wswitch-default;-Wundef;-Wno-unused>"
  "$<$<C_COMPILER_ID:MSVC>:/W4>"
  )

# set message to color
ADD_COMPILE_OPTIONS(
  "$<$<C_COMPILER_ID:Clang>:-fdiagnostics-color=always>"
  "$<$<C_COMPILER_ID:GNU>:-fdiagnostics-color=always>"
  )

ADD_COMPILE_OPTIONS(-O9 -g -fomit-frame-pointer)
ADD_DEFINITIONS(-D_GNU_SOURCE -DTARGET_BIG_ENDIAN=0)
ADD_EXECUTABLE(${PROJECT_NAME}
    pcl3gui2png.c
    ../src/sgstring.c
    ../modules/printer/decompress.c
    ../modules/printer/dj460interp.c
    ../modules/printer/pcl3.c
    ../modules/printer/pcl3gui.c
    ../modules/printer/pjl.c
    ../modules/printer/printengine.c
)

TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/../src")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/../modules/printer")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/")
