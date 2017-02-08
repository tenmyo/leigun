CMAKE_MINIMUM_REQUIRED(VERSION 3.1)

PROJECT(leigun
  VERSION 0.1.0
  LANGUAGES C
  )

ADD_EXECUTABLE(${PROJECT_NAME}
    softgun.c
    
    alsasound.c
    bus.c
#    bus64.c
    clock.c
    configfile.c
    crc16.c
    crc32.c
    crc8.c
    cycletimer.c
    debugvars.c
    diskimage.c
    dram.c
    elfloader.c
    fbdisplay.c
    filesystem.c
    hello_world.c
    i2c_serdes.c
    ihex.c
    keyboard.c
    loader.c
    logical.c
    mouse.c
    nand.c
    nullsound.c
    relais.c
    rfbserver.c
    rom.c
    rtc.c
#    sdldisplay.c
    senseless.c
    serial.c
    sglib.c
    sgstring.c
    signode.c
    sound.c
    spidevice.c
    sram.c
    srec.c
    strhash.c
    throttle.c
    usbdevice.c
    usbstdrq.c
    xy_hash.c
    xy_tree.c
    
    version.c
    core/asyncmanager.c
    core/device.c
    core/exithandler.c
    core/lib.c
    core/logging.c
    core/str.c
    boards/boards.c
    )


CONFIGURE_FILE("${PROJECT_SOURCE_DIR}/version.h.in" "${PROJECT_BINARY_DIR}/version.h")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${PROJECT_BINARY_DIR}/")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/")

FIND_PACKAGE(PkgConfig)

# zlib
FIND_PACKAGE(ZLIB REQUIRED)
IF (ZLIB_FOUND)
    TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} SYSTEM PUBLIC ${ZLIB_INCLUDE_DIRS})
    TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC ZLIB::ZLIB)
ENDIF (ZLIB_FOUND)

# ALSA
FIND_PACKAGE(ALSA REQUIRED)
IF (ALSA_FOUND)
    TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} SYSTEM PUBLIC ${ALSA_INCLUDE_DIR})
    TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC ${ALSA_LIBRARIES})
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
TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC ${LIBUV_LIBRARIES})

SET_PROPERTY(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 99)
SET_PROPERTY(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD_REQUIRED ON)


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

TARGET_LINK_LIBRARIES(
  ${PROJECT_NAME} PRIVATE
  "$<$<C_COMPILER_ID:Clang>:-rdynamic>"
  "$<$<C_COMPILER_ID:GNU>:-rdynamic>"
  )

TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE -D_GNU_SOURCE -DTARGET_BIG_ENDIAN=0)

INSTALL(TARGETS ${PROJECT_NAME} DESTINATION bin)
