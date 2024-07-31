cmake_minimum_required(VERSION 3.10)
project(lvgl)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-fPIC -Wall -Wno-error -g)

find_program(CCACHE_PROGRAM ccache)

if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

include_directories(
    include
    include/libdrm
    lv_fb
    lv_fb/rlottie/inc
    lv_fb/lvgl/src
    generated
    custom
    generated/guider_customer_fonts
    generated/guider_fonts
    generated/images
    observer/include
    src
)

link_directories(${CMAKE_SOURCE_DIR}/lib)

option(ENABLE_LOTTIE_SUPPORT "Enable Lottie support" OFF)

if(ENABLE_LOTTIE_SUPPORT)
    add_subdirectory(rlottie)
endif()

add_subdirectory(lv_fb/lvgl)
add_subdirectory(lv_fb/lv_drivers)

add_definitions(-DUSE_DRM -DUSE_EVDEV)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_DRM -DUSE_EVDEV")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_DRM -DUSE_EVDEV")

target_include_directories(lvgl PUBLIC ${PROJECT_SOURCE_DIR} rlottie/inc/)

FILE(GLOB_RECURSE app_sources generated/*.c custom/*.c src/*.c)

add_executable(lvgl_app ${app_sources})

target_compile_definitions(lvgl_app PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)

target_link_libraries(lvgl_app
    lvgl
    lv_drivers
    lvgl::drivers
    lvgl::examples
    lvgl::demos
    m
    drm
    pthread
    -static
    -lstdc++
)

if(ENABLE_LOTTIE_SUPPORT)
    target_link_libraries(lvgl_app rlottie::rlottie)
endif()

