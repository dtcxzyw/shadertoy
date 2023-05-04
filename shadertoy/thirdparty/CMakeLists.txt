set(IMGUI_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/imgui)
add_library(imgui
    ${IMGUI_SRC_DIR}/imgui_demo.cpp
    ${IMGUI_SRC_DIR}/imgui_draw.cpp
    ${IMGUI_SRC_DIR}/imgui_tables.cpp
    ${IMGUI_SRC_DIR}/imgui_widgets.cpp
    ${IMGUI_SRC_DIR}/imgui_internal.h
    ${IMGUI_SRC_DIR}/imgui.cpp
    ${IMGUI_SRC_DIR}/imgui.h
    ${IMGUI_SRC_DIR}/misc/cpp/imgui_stdlib.cpp
    ${IMGUI_SRC_DIR}/misc/cpp/imgui_stdlib.h
    )
target_include_directories(imgui PUBLIC ${IMGUI_SRC_DIR})

set(HELLOIMGUI_USE_GLFW_OPENGL3 ON)
set(HELLOIMGUI_BUILD_IMGUI OFF)
set(HELLOIMGUI_USE_GLFW_OPENGL3 OFF)
set(HELLOIMGUI_IMGUI_SOURCE_DIR ${IMGUI_SRC_DIR} PARENT_SCOPE)
add_subdirectory(hello_imgui)
list(APPEND CMAKE_MODULE_PATH ${HELLOIMGUI_CMAKE_PATH})
include(hello_imgui_add_app)

file(GLOB IMGUI_NODE_EDITOR_SRC imgui-node-editor/*.cpp)
set(IMGUI_NODE_EDITOR_SRC ${IMGUI_NODE_EDITOR_SRC} PARENT_SCOPE)
set(IMGUI_NODE_EDITOR_INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR} PARENT_SCOPE)
