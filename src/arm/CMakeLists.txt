CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(softgun_arm C)

ADD_LIBRARY(${PROJECT_NAME}
    arm_vfp.c
    arm9cpu.c
    idecode_arm.c
    instructions_arm.c
    mmu_1176.c
    mmu_a8.c
    mmu_arm.c
    mmu_arm9.c
    thumb_decode.c
    thumb_instructions.c
    )

TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/core")

