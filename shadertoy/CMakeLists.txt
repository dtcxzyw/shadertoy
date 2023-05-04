file(GLOB SHADERTOY_SRC "*.cpp" "NodeEditor/*.cpp")

find_package(fmt CONFIG REQUIRED)
find_package(GLEW CONFIG REQUIRED)
find_package(unofficial-nativefiledialog CONFIG REQUIRED)
find_package(Microsoft.GSL CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_path(CPP_BASE64_INCLUDE_DIRS "cpp-base64/base64.cpp")

add_subdirectory(thirdparty)

hello_imgui_add_app(shadertoy 
	${SHADERTOY_SRC} 
	${CMAKE_CURRENT_LIST_DIR}/thirdparty/ImGuiColorTextEdit/TextEditor.cpp 
	${CPP_BASE64_INCLUDE_DIRS}/cpp-base64/base64.cpp 
	${IMGUI_NODE_EDITOR_SRC}
)
target_include_directories(shadertoy PRIVATE ${CMAKE_CURRENT_LIST_DIR}/thirdparty/hello_imgui/src ${HELLOIMGUI_IMGUI_SOURCE_DIR} ${CMAKE_CURRENT_LIST_DIR}/thirdparty/)
target_link_libraries(shadertoy PRIVATE fmt::fmt GLEW::GLEW unofficial::nativefiledialog::nfd Microsoft.GSL::GSL magic_enum::magic_enum)
target_include_directories(shadertoy PRIVATE ${Stb_INCLUDE_DIR} ${CPP_BASE64_INCLUDE_DIRS} ${IMGUI_NODE_EDITOR_INCLUDE_DIRS})
add_custom_command(TARGET shadertoy PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy 
	${CMAKE_CURRENT_LIST_DIR}/thirdparty/imgui-node-editor/examples/blueprints-example/data/BlueprintBackground.png 
	${CMAKE_BINARY_DIR}/assets/BlueprintBackground.png)
