cmake_minimum_required(VERSION 3.25.0)
project(PycoLang VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES main.c pyco_compiler.c)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

