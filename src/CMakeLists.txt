CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

PROJECT(softgun
  VERSION 0.22.1
  LANGUAGES C
  )

ADD_EXECUTABLE(${PROJECT_NAME}
    softgun.c
    
    alsasound.c
    bus.c
#    bus64.c
    cleanup.c
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
    shlib.c
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
    PKG_CHECK_MODULES(LIBUV libuv>=1.0.0)
    IF (LIBUV_FOUND)
        INCLUDE_DIRECTORIES(${LIBUV_INCLUDE_DIRS})
        LINK_DIRECTORIES(${LIBUV_LIBRARY_DIRS})
    ENDIF (LIBUV_FOUND)
ENDIF (PKG_CONFIG_FOUND)
IF (NOT LIBUV_FOUND)
    FIND_PACKAGE(LibUV)
    IF (LIBUV_FOUND AND LIBUV_VERSION VERSION_LESS "1.0.0")
        MESSAGE(STATUS "libuv found but ignored; is too old")
        UNSET(LIBUV_FOUND)
    ENDIF ()
    IF (LIBUV_FOUND)
        INCLUDE_DIRECTORIES(${LIBUV_INCLUDE_DIR})
    ENDIF (LIBUV_FOUND)
ENDIF (NOT LIBUV_FOUND)
IF (NOT LIBUV_FOUND)
    SET(LIBUV_LIBRARIES -luv)
ENDIF (NOT LIBUV_FOUND)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC ${LIBUV_LIBRARIES})

SET_PROPERTY(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 99)
SET_PROPERTY(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD_REQUIRED ON)

TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC ${CMAKE_DL_LIBS} rt m)


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
  "$<$<C_COMPILER_ID:Clang>:-Wno-format-pedantic;-Wno-padded;-Wno-reserved-id-macro;-Wno-unused-parameter;-Wno-unused-macros>"
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

TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE -D_GNU_SOURCE -DTARGET_BIG_ENDIAN=0)
