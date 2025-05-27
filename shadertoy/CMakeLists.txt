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
	set_source_files_properties(${SHADERTOY_SRC} PROPERTIES COMPILE_FLAGS "/Wall /WX /wd4514 /wd4820 /wd4623 /wd4625 /wd4626 /wd5026 /wd5027 /wd4061 /wd5045 /wd4365 /wd4868 /wd4710 /wd5262 /wd4711 /wd4738 /D_CRT_SECURE_NO_WARNINGS /D_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS")
elseif(CMAKE_COMPILER_IS_GNUCXX)
	set_source_files_properties(${SHADERTOY_SRC} PROPERTIES COMPILE_FLAGS "-Wall -Wextra -Werror -Wconversion -Wshadow -Wno-psabi -Wno-array-bounds")
endif()
target_include_directories(shadertoy PRIVATE ${CMAKE_CURRENT_LIST_DIR}/thirdparty/hello_imgui/src ${IMGUI_SRC_DIR} ${CMAKE_CURRENT_LIST_DIR}/thirdparty/)
target_link_libraries(shadertoy PRIVATE fmt::fmt GLEW::GLEW unofficial::nativefiledialog::nfd Microsoft.GSL::GSL magic_enum::magic_enum httplib::httplib OpenSSL::SSL OpenSSL::Crypto)
target_include_directories(shadertoy PRIVATE ${Stb_INCLUDE_DIR} ${CPP_BASE64_INCLUDE_DIRS} ${IMGUI_NODE_EDITOR_INCLUDE_DIRS})

if(APPLE)
	find_library(AppleSecurityLib Security)
	target_link_libraries(shadertoy PRIVATE ${AppleSecurityLib})
#   set_target_properties(shadertoy PROPERTIES 
#	    MACOSX_BUNDLE TRUE
#		MACOSX_BUNDLE_BUNDLE_NAME "Shadertoy live viewer"
#		MACOSX_BUNDLE_BUNDLE_VERSION ${CMAKE_PROJECT_VERSION}
#		MACOSX_BUNDLE_SHORT_VERSION_STRING ${CMAKE_PROJECT_VERSION}
#		MACOSX_BUNDLE_COPYRIGHT "Copyright 2023-2025 Yingwei Zheng"
#		MACOSX_BUNDLE_GUI_IDENTIFIER "com.dtcxzyw.shadertoy"
#       MACOSX_FRAMEWORK_IDENTIFIER "com.dtcxzyw.shadertoy"
#	)
endif(APPLE)

set(SHADERTOY_BACKGROUND_IMG ${CMAKE_CURRENT_LIST_DIR}/thirdparty/imgui-node-editor/examples/blueprints-example/data/BlueprintBackground.png )
add_custom_command(TARGET shadertoy PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy 
	${SHADERTOY_BACKGROUND_IMG}
	${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/BlueprintBackground.png)

install(FILES ${SHADERTOY_BACKGROUND_IMG} DESTINATION assets)
install(FILES ${CMAKE_SOURCE_DIR}/LICENSE DESTINATION .)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/licenses DESTINATION .)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/examples DESTINATION .)
install(TARGETS shadertoy DESTINATION .)

# if(APPLE)
# 	include(InstallRequiredSystemLibraries)
# 	set(CPACK_GENERATOR "DRAGNDROP")
#     set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)
# 	set(CPACK_PACKAGE_VENDOR "Yingwei Zheng")
# 	include(CPack)
# endif(APPLE)
