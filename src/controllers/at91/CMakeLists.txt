CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(softgun_controller_at91 C)

ADD_LIBRARY(${PROJECT_NAME} OBJECT
    at91_aic.c
    at91_ebi.c
    at91_ebinand.c
    at91_ecc.c
    at91_emac.c
    at91_emacb.c
    at91_lcdc.c
    at91_matrix.c
    at91_mc.c
    at91_mci.c
    at91_pdc.c
    at91_pio.c
    at91_pit.c
    at91_pmc.c
    at91_rstc.c
    at91_sdramc.c
    at91_spi.c
    at91_st.c
    at91_tc.c
    at91_twi.c
    at91_usart.c
    at91_wdt.c
    at91sam7se_spi.c
    at91sam_efc.c
    )

TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/core")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/devices/phy")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/devices/sdcard")

