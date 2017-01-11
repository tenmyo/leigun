CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
CMAKE_POLICY(SET CMP0048 NEW)

PROJECT(softgun VERSION 0.22.1 LANGUAGES C)
#SET(CMAKE_VERBOSE_MAKEFILE 1)
#SET(CMAKE_C_COMPILER /usr/bin/gcc)

ADD_COMPILE_OPTIONS(-Wall -O9 -g -fomit-frame-pointer -fno-strict-overflow -Wstrict-overflow=3 -Wno-unused-but-set-variable)
ADD_COMPILE_OPTIONS(-fdiagnostics-color=always)

ADD_DEFINITIONS(-D_GNU_SOURCE -DTARGET_BIG_ENDIAN=0)
CONFIGURE_FILE("${CMAKE_SOURCE_DIR}/version.h.in" "${CMAKE_BINARY_DIR}/version.h")

#ADD_SUBDIRECTORY(core)
#ADD_SUBDIRECTORY(busdevices)

ADD_SUBDIRECTORY(boards)
#ADD_SUBDIRECTORY(cli)
ADD_SUBDIRECTORY(gdb)

ADD_SUBDIRECTORY(arm)
ADD_SUBDIRECTORY(avr8)
ADD_SUBDIRECTORY(c16x)
ADD_SUBDIRECTORY(coldfire)
#ADD_SUBDIRECTORY(m16c)
#ADD_SUBDIRECTORY(m32c)
ADD_SUBDIRECTORY(mcs51)
ADD_SUBDIRECTORY(mips64)
ADD_SUBDIRECTORY(mos6510)
ADD_SUBDIRECTORY(ppc)
#ADD_SUBDIRECTORY(rx)
#ADD_SUBDIRECTORY(sh4)

ADD_SUBDIRECTORY(controllers)
ADD_SUBDIRECTORY(devices)
#ADD_SUBDIRECTORY(float)
ADD_SUBDIRECTORY(printer)

ADD_EXECUTABLE(${PROJECT_NAME}
    softgun.c
    pl190_irq.c
    xy_tree.c
    xy_hash.c
    bus.c
    signode.c
    ste10_100.c
    m93c46.c
    linux-tap.c
    dram.c
    sram.c
    amdflash.c
    usb_ohci.c
    cycletimer.c
    configfile.c
    ml6652.c
    elfloader.c
    loader.c
    i2c_serdes.c
    i82559.c
    pc16550.c
    rom.c
    ihex.c
    srec.c
    shlib.c
    diskimage.c
    aitc.c
    imx_timer.c
    serial.c
    cs8900.c
    clock.c
    rfbserver.c
    sht71.c
    keyboard.c
    matrix_keyboard.c
    usbdevice.c
    usbstdrq.c
    djet460.c
    relais.c
    senseless.c
    hello_world.c
    logical.c
    sgstring.c
    parport_pc.c
    crc32.c
    tx49xx_sio.c
    isp1105.c
    filesystem.c
    nullsound.c
    alsasound.c
    sound.c
    fbdisplay.c
    strhash.c
    debugvars.c
    debugger.c
    sglib.c
    crc8.c
    spidevice.c
    jtag_tap.c
    crc16.c
    mmcdev.c
    nand.c
    throttle.c
    cleanup.c
    rtc.c
    mouse.c
    
    version.c
    core/asyncmanager.c
    )

TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_BINARY_DIR}/")

FIND_PACKAGE(PkgConfig)

# zlib
FIND_PACKAGE(ZLIB REQUIRED)
IF (ZLIB_FOUND)
    TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} SYSTEM PUBLIC ${ZLIB_INCLUDE_DIRS})
    TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${ZLIB_LIBRARIES})
ENDIF (ZLIB_FOUND)

# ALSA
FIND_PACKAGE(ALSA REQUIRED)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} SYSTEM PUBLIC ${ALSA_INCLUDE_DIR})

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

TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/core")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/busdevices")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/arm")
#TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/cli")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/gdb")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/printer")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/controllers/ns9750")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/controllers/imx21")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/devices/phy")

SET_PROPERTY(TARGET softgun PROPERTY C_STANDARD 99)
SET_PROPERTY(TARGET softgun PROPERTY C_STANDARD_REQUIRED ON)


TARGET_LINK_LIBRARIES(${PROJECT_NAME}
    -Wl,-whole-archive
    softgun_boards
    softgun_device
    -Wl,-no-whole-archive
#    softgun_cli
    softgun_gdb
    softgun_controller
    softgun_printer
    softgun_arm
    softgun_avr8
    softgun_c16x
    softgun_coldfire
#    softgun_m16c
#    softgun_m32c
    softgun_mcs51
    softgun_mips64
    softgun_mos6510
    softgun_ppc
#    softgun_rx
#    softgun_sh4
#    softgun_float
    #softgun_busdevices
    #softgun_core
    pthread
    rt
    m
    ${ZLIB_LIBRARIES}
    ${ALSA_LIBRARY}
    dl
    )


PROJECT(pcl3gui2png C)
ADD_EXECUTABLE(${PROJECT_NAME}
    sgstring.c
    printer/decompress.c
    printer/dj460interp.c
    printer/pcl3.c
    printer/pcl3gui.c
    printer/pcl3gui2png.c
    printer/pjl.c
    printer/printengine.c
)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/printer")
