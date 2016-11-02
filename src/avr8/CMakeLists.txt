CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(softgun_avr8 C)

ADD_LIBRARY(${PROJECT_NAME}
    atm128_timer0.c
    atm644_extint.c
    atm644_spi.c
    atm644_sysreset.c
    atm644_timer02.c
    atm644_timer1.c
    atm644_twi.c
    atm644_usart.c
    avr8_adc.c
    avr8_cpu.c
    avr8_eeprom.c
    avr8_gpio.c
    avr8_port.c
    idecode_avr8.c
    instructions_avr8.c
    uze_snes.c
    uze_timer2.c
    uze_video.c
    xmegaA_clock.c
    xmegaA_dma.c
    xmegaA_pmic.c
    xmegaA_timer.c
    xmegaA_twi.c
    xmegaA_usart.c
    )

TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/core")

