cmake_minimum_required(VERSION 3.0)
project(finalProject)

find_package(OpenGL REQUIRED)
set (CMAKE_CXX_STANDARD 11)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")


add_subdirectory(external)

include_directories(
	external/glfw-3.1.2/include/
	external/glm-0.9.7.1/
	external/glad-opengl-3.3/include/
	external/tinygltf-2.9.3/
	external/
	src/
)

add_executable(project_scene
	src/lab4_character.cpp
	src/camera.cpp
	src/skybox.cpp
	src/render/shader.cpp
	src/tiny_gltf.cpp
	src/entity.cpp
	src/terrain.cpp
)
target_link_libraries(project_scene
	${OPENGL_LIBRARY}
	glfw
	glad
)