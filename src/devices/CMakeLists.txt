CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
IF(POLICY CMP0048)
    CMAKE_POLICY(SET CMP0048 OLD)
ENDIF(POLICY CMP0048)

PROJECT(softgun_device C)

SET(subprojects "")
FILE(GLOB dirs RELATIVE ${PROJECT_SOURCE_DIR} */)
FOREACH(dir ${dirs})
    IF(IS_DIRECTORY "${PROJECT_SOURCE_DIR}/${dir}")
        IF(EXISTS "${PROJECT_SOURCE_DIR}/${dir}/CMakeLists.txt")
            ADD_SUBDIRECTORY(${dir})
            SET(subprojects ${subprojects} $<TARGET_OBJECTS:${PROJECT_NAME}_${dir}>)
        ENDIF()
    ENDIF()
ENDFOREACH()

ADD_LIBRARY(${PROJECT_NAME} ${subprojects})
