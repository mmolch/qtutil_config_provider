cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME "mmolch_qtutil_config_provider")
set(PROJECT_VERSION "0.1.0")

project(${PROJECT_NAME}
    VERSION ${PROJECT_VERSION}
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(libs/mmolch_qtutil_json)

find_package(Qt6 6.4 REQUIRED COMPONENTS
    Core
)

qt_standard_project_setup()

add_library(${PROJECT_NAME} STATIC
    include/mmolch/qtutil_config_provider.h src/qtutil_config_provider.cpp
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    Qt6::Core
    mmolch_qtutil_json
)

target_include_directories(${PROJECT_NAME} PUBLIC
    include
)

if(CMAKE_CURRENT_LIST_DIR STREQUAL ${CMAKE_SOURCE_DIR})
    if (CMAKE_BUILD_TYPE MATCHES "Debug")
        target_compile_options(${PROJECT_NAME} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    endif ()

    if(BUILD_EXAMPLE)
        add_subdirectory(example)
    endif()
endif()
