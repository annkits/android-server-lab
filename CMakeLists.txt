cmake_minimum_required(VERSION 3.14)
project(backend_notes)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(THIRD_PARTY_DIR ${PROJECT_SOURCE_DIR}/third_party)
set(SRC_DIR ${PROJECT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(CURL REQUIRED)

include_directories(${SDL2_INCLUDE_DIRS})
include_directories(${INCLUDE_DIR})
include_directories(${GLEW_INCLUDE_DIRS})
include_directories(${CURL_INCLUDE_DIRS})

find_package(PkgConfig REQUIRED)
find_package(CURL REQUIRED)
pkg_check_modules(ZMQ REQUIRED IMPORTED_TARGET libzmq)
find_package(nlohmann_json 3.11 REQUIRED)

add_library(imgui STATIC
    ${THIRD_PARTY_DIR}/imgui/imgui.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_draw.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_demo.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_tables.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_widgets.cpp
    ${THIRD_PARTY_DIR}/imgui/backends/imgui_impl_sdl2.cpp
    ${THIRD_PARTY_DIR}/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${THIRD_PARTY_DIR}/imgui)
target_compile_options(imgui PRIVATE -fPIC)

add_library(implot STATIC
    ${THIRD_PARTY_DIR}/implot/implot.cpp
    ${THIRD_PARTY_DIR}/implot/implot_demo.cpp
    ${THIRD_PARTY_DIR}/implot/implot_items.cpp
)
target_include_directories(implot PUBLIC ${THIRD_PARTY_DIR}/implot)
target_link_libraries(implot PRIVATE imgui)
target_compile_options(implot PRIVATE -fPIC)

add_executable(backend
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/server.cpp
    ${SRC_DIR}/gui.cpp
    ${SRC_DIR}/curl_utils.cpp    
    ${SRC_DIR}/osm_map.cpp
    ${SRC_DIR}/tile_manager.cpp
    ${SRC_DIR}/map.cpp
    )

target_include_directories(backend PRIVATE
    ${INCLUDE_DIR}
    ${SDL2_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
    ${ZMQ_INCLUDE_DIRS}
    ${PostgreSQL_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS} 
    # include
)

target_link_libraries(backend PRIVATE
    imgui 
    implot 
    PkgConfig::ZMQ
    nlohmann_json::nlohmann_json
    ${SDL2_LIBRARIES} 
    ${OPENGL_LIBRARIES} 
    ${GLEW_LIBRARIES}
    PostgreSQL::PostgreSQL
    ${CURL_LIBRARIES}
)