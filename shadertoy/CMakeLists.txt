file(GLOB SHADERTOY_SRC "*.cpp")

find_package(fmt CONFIG REQUIRED)
find_package(GLEW CONFIG REQUIRED)
find_package(unofficial-nativefiledialog CONFIG REQUIRED)
find_package(Microsoft.GSL CONFIG REQUIRED)

add_subdirectory(thirdparty)

hello_imgui_add_app(shadertoy ${SHADERTOY_SRC} ${CMAKE_CURRENT_LIST_DIR}/thirdparty/ImGuiColorTextEdit/TextEditor.cpp)
target_include_directories(shadertoy PRIVATE ${CMAKE_CURRENT_LIST_DIR}/thirdparty/hello_imgui/src ${HELLOIMGUI_IMGUI_SOURCE_DIR} ${CMAKE_CURRENT_LIST_DIR}/thirdparty/)
target_link_libraries(shadertoy PRIVATE fmt::fmt GLEW::GLEW unofficial::nativefiledialog::nfd Microsoft.GSL::GSL)
