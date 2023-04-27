file(GLOB_RECURSE SHADERTOY_SRC "*.cpp")

find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(GLEW CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(shadertoy ${SHADERTOY_SRC})
target_link_libraries(shadertoy PRIVATE glfw imgui::imgui fmt::fmt GLEW::GLEW OpenGL::GL)
