cmake_minimum_required(VERSION 3.16)

if(DEFINED DEPENDENCIES_DIR)
        include(${DEPENDENCIES_DIR}/modorganizer_super/cmake_common/mo2.cmake)
else()
	include(${CMAKE_CURRENT_LIST_DIR}/../cmake_common/mo2.cmake)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(example_plugin LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

mo2_configure_plugin(
        ${PROJECT_NAME}
        WARNINGS OFF
        PRIVATE_DEPENDS uibase
)
    
mo2_install_target(${PROJECT_NAME})
 