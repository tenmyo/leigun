CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
IF(POLICY CMP0048)
    CMAKE_POLICY(SET CMP0048 OLD)
ENDIF(POLICY CMP0048)

PROJECT(softgun_m32c C)

ADD_LIBRARY(${PROJECT_NAME}
    cpu_m32c.c
    idecode_m32c.c
    instructions_m32c.c
    adc_m32c.c
    can_m32c.c
    flash_m32c.c
    iio_m32c.c
    irq_m32c87.c
    m32c_crc.c
    pio_m32c.c
    sysco_m32c.c
    timer_m32c.c
    uart56_m32c.c
    uart_m32c.c
    )

TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/core")

