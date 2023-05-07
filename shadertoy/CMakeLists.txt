file(GLOB SHADERTOY_SRC "*.cpp" "NodeEditor/*.cpp")

find_package(fmt CONFIG REQUIRED)
find_package(GLEW CONFIG REQUIRED)
find_package(unofficial-nativefiledialog CONFIG REQUIRED)
find_package(Microsoft.GSL CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_path(CPP_BASE64_INCLUDE_DIRS "cpp-base64/base64.cpp")

add_subdirectory(thirdparty)

hello_imgui_add_app(shadertoy 
	${SHADERTOY_SRC} 
	${CMAKE_CURRENT_LIST_DIR}/thirdparty/ImGuiColorTextEdit/TextEditor.cpp 
	${CPP_BASE64_INCLUDE_DIRS}/cpp-base64/base64.cpp 
	${IMGUI_NODE_EDITOR_SRC}
)
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	set_source_files_properties(${SHADERTOY_SRC} PROPERTIES COMPILE_FLAGS "/Wall /WX /wd4514 /wd4820 /wd4623 /wd4625 /wd4626 /wd5026 /wd5027 /wd4061 /wd5045 /wd4365 /wd4868 /wd4710 /wd5262 /wd4711 /D_CRT_SECURE_NO_WARNINGS")
elseif(CMAKE_COMPILER_IS_GNUCXX)
	set_source_files_properties(${SHADERTOY_SRC} PROPERTIES COMPILE_FLAGS "-Wall -Wextra -Werror -Wconversion -Wshadow -Wno-psabi")
endif()
target_include_directories(shadertoy PRIVATE ${CMAKE_CURRENT_LIST_DIR}/thirdparty/hello_imgui/src ${IMGUI_SRC_DIR} ${CMAKE_CURRENT_LIST_DIR}/thirdparty/)
target_link_libraries(shadertoy PRIVATE fmt::fmt GLEW::GLEW unofficial::nativefiledialog::nfd Microsoft.GSL::GSL magic_enum::magic_enum httplib::httplib OpenSSL::SSL OpenSSL::Crypto)
target_include_directories(shadertoy PRIVATE ${Stb_INCLUDE_DIR} ${CPP_BASE64_INCLUDE_DIRS} ${IMGUI_NODE_EDITOR_INCLUDE_DIRS})
add_custom_command(TARGET shadertoy PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy 
	${CMAKE_CURRENT_LIST_DIR}/thirdparty/imgui-node-editor/examples/blueprints-example/data/BlueprintBackground.png 
	${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/BlueprintBackground.png)
add_custom_command(TARGET shadertoy PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy 
	${CMAKE_SOURCE_DIR}/LICENSE
	${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/LICENSE)
add_custom_command(TARGET shadertoy PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_SOURCE_DIR}/licenses 
	${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/licenses)
add_custom_command(TARGET shadertoy PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_SOURCE_DIR}/examples 
	${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/examples)

install(DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ DESTINATION ${CMAKE_INSTALL_PREFIX})
