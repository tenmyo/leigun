CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
IF(POLICY CMP0048)
    CMAKE_POLICY(SET CMP0048 OLD)
ENDIF(POLICY CMP0048)

PROJECT(softgun_rx C)

ADD_LIBRARY(${PROJECT_NAME}
    ada_rx.c
    adb_rx.c
    can_rx63n.c
    clk_rx111.c
    clk_rx62n.c
    clk_rx63n.c
    cmt_rx62n.c
    cpu_rx.c
    crc_rx.c
    da_rx63.c
    etherc_rx62n.c
    flash_rx.c
    flash_rx111.c
    flash_rx63n.c
    i2c_rx62t.c
    icu_rx62n.c
    idecode_rx.c
    instructions_rx.c
    ioport_rx62n.c
    ioport_rx63n.c
    iwdt_rx111.c
    mpc_rx63.c
    rspi_rx62n.c
    rtc_rx63n.c
    s12ada_rx63n.c
    sci_rx62n.c
    usb_rx.c
    wdt_rx62n.c
    wdta_rx63n.c
    )

TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/core")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/float")

