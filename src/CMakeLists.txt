CMAKE_MINIMUM_REQUIRED(VERSION 3.1)

PROJECT(leigun
  VERSION 0.1.0
  LANGUAGES C
  )

## Add the project files
# source files
ADD_EXECUTABLE(${PROJECT_NAME}
    softgun/softgun.c
    
    softgun/alsasound.c
    softgun/bus.c
#    bus64.c
    softgun/clock.c
    softgun/configfile.c
    softgun/crc16.c
    softgun/crc32.c
    softgun/crc8.c
    softgun/cycletimer.c
    softgun/debugvars.c
    softgun/diskimage.c
    softgun/dram.c
    softgun/elfloader.c
    softgun/fbdisplay.c
    softgun/filesystem.c
    softgun/hello_world.c
    softgun/i2c_serdes.c
    softgun/ihex.c
    softgun/keyboard.c
    softgun/loader.c
    softgun/logical.c
    softgun/mouse.c
    softgun/nand.c
    softgun/nullsound.c
    softgun/relais.c
    softgun/rfbserver.c
    softgun/rtc.c
#    softgun/sdldisplay.c
    softgun/senseless.c
    softgun/serial.c
    softgun/sglib.c
    softgun/sgstring.c
    softgun/signode.c
    softgun/sound.c
    softgun/spidevice.c
    softgun/sram.c
    softgun/srec.c
    softgun/strhash.c
    softgun/throttle.c
    softgun/usbdevice.c
    softgun/usbstdrq.c
    softgun/xy_hash.c
    softgun/xy_tree.c
    
    version.c
    
    asyncmanager.c
    device.c
    exithandler.c
    globalclock.c
    lib.c
    logging.c
    str.c
    timerlist.c
    
    memmapped/sram.c
    )

# generated files
CONFIGURE_FILE("${PROJECT_SOURCE_DIR}/version.h.in" "${PROJECT_BINARY_DIR}/version.h")

# header files
FILE(GLOB files RELATIVE ${PROJECT_SOURCE_DIR} *.h core/*.h)
SET_PROPERTY(TARGET ${PROJECT_NAME} PROPERTY PUBLIC_HEADER ${files})
SET_PROPERTY(TARGET ${PROJECT_NAME} APPEND PROPERTY PUBLIC_HEADER
  "${PROJECT_BINARY_DIR}/version.h"
)


## Set the required
# The standards
# Note: These commands require the CMake 3.1+.
SET_PROPERTY(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 99)
SET_PROPERTY(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD_REQUIRED ON)
TARGET_COMPILE_FEATURES(${PROJECT_NAME} PUBLIC c_function_prototypes)
TARGET_COMPILE_FEATURES(${PROJECT_NAME} PUBLIC c_variadic_macros)

# Automatically add the current source- and build directories to the include path.
SET(CMAKE_INCLUDE_CURRENT_DIR ON)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/softgun")

FIND_PACKAGE(PkgConfig)

# zlib
FIND_PACKAGE(ZLIB REQUIRED)
IF (ZLIB_FOUND)
    TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} SYSTEM PRIVATE ${ZLIB_INCLUDE_DIRS})
    TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE ${ZLIB_LIBRARIES})
ENDIF (ZLIB_FOUND)

# ALSA
FIND_PACKAGE(ALSA REQUIRED)
IF (ALSA_FOUND)
    TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} SYSTEM PRIVATE ${ALSA_INCLUDE_DIR})
    TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE ${ALSA_LIBRARIES})
ENDIF (ALSA_FOUND)

# libuv
IF (PKG_CONFIG_FOUND)
    PKG_CHECK_MODULES(LIBUV libuv>=1.10.0)
    IF (LIBUV_FOUND)
      SET(LIBUV_LIBRARIES ${LIBUV_LDFLAGS} ${LIBUV_LDFLAGS_OTHER})
    ENDIF (LIBUV_FOUND)
ENDIF (PKG_CONFIG_FOUND)
IF (NOT LIBUV_FOUND)
    FIND_PATH(LIBUV_INCLUDE_DIRS NAMES uv.h)
    FIND_LIBRARY(LIBUV_LIBRARIES NAMES uv libuv)
    IF (LIBUV_LIBRARIES)
      SET(LIBUV_FOUND 1)
    ENDIF ()
ENDIF (NOT LIBUV_FOUND)
IF (NOT LIBUV_FOUND)
    MESSAGE(STATUS "libuv not found")
    SET(LIBUV_LIBRARIES -luv)
ENDIF ()
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} SYSTEM PUBLIC ${LIBUV_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE ${LIBUV_LIBRARIES})


# ENABLE WARNINGS
TARGET_COMPILE_OPTIONS(
  ${PROJECT_NAME} PRIVATE
  "$<$<C_COMPILER_ID:Clang>:-Wall;-Weverything>"
  "$<$<C_COMPILER_ID:GNU>:-pedantic;-Wall;-Wextra;-Wcast-align;-Wcast-qual;-Wdisabled-optimization;-Wformat=2;-Winit-self;-Wlogical-op;-Wmissing-declarations;-Wmissing-include-dirs;-Wredundant-decls;-Wshadow;-Wsign-conversion;-Wstrict-overflow=5;-Wswitch-default;-Wundef>"
  "$<$<C_COMPILER_ID:MSVC>:/W4>"
  )

# DISABLE FUSSY WARNINGS
TARGET_COMPILE_OPTIONS(
  ${PROJECT_NAME} PRIVATE
  "$<$<C_COMPILER_ID:Clang>:-Wno-padded;-Wno-unused-parameter;-Wno-unused-macros>"
  "$<$<C_COMPILER_ID:GNU>:-Wno-unused-parameter>"
  "$<$<C_COMPILER_ID:MSVC>:>"
  )

# set message to color
IF("Ninja" STREQUAL ${CMAKE_GENERATOR})
  TARGET_COMPILE_OPTIONS(
    ${PROJECT_NAME} PRIVATE
    "$<$<C_COMPILER_ID:Clang>:-fdiagnostics-color=always>"
    "$<$<C_COMPILER_ID:GNU>:-fdiagnostics-color=always>"
    )
ENDIF()

SET_PROPERTY(TARGET ${PROJECT_NAME} PROPERTY ENABLE_EXPORTS ON)

TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE -D_GNU_SOURCE -DTARGET_BIG_ENDIAN=0)

INSTALL(TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}-export
  RUNTIME       DESTINATION bin
  LIBRARY       DESTINATION lib
  INCLUDES      DESTINATION include/
  PUBLIC_HEADER DESTINATION include/${PROJECT_NAME})

INSTALL(EXPORT ${PROJECT_NAME}-export
  FILE ${PROJECT_NAME}-config.cmake
  DESTINATION cmake/
  EXPORT_LINK_INTERFACE_LIBRARIES)
