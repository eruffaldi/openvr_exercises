set(OPENVR ${PROJECT_SOURCE_DIR}/openvr)
include_directories(${OPENVR}/headers)
find_package(cmakego COMPONENTS glew REQUIRED)
if(CMAKE_COMPILER_IS_GNUCXX)
add_definitions(--std=c++11)
else()
endif()

option(GLFWMODE "use glfw instead of sdl" OFF)

find_package(cmakego COMPONENTS sdl2 REQUIRED)

if(GLFWMODE)
find_package(cmakego COMPONENTS glfw REQUIRED)
set(EXTRALIB p::glfw)
add_definitions(-DGLFWMODE)
else()
set(EXTRALIB p::sdl2)
endif()

if(APPLE)
set(OPENVR_OS osx32)
add_definitions(-DOSX -DAPPLE)
set(EXTRASRC src/osxfilebridge.mm)
elseif(LINUX)
set(OPENVR_OS linux32)
elseif(WIN32)
if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
set(OPENVR_OS win64)
else()
set(OPENVR_OS win32)
endif()
endif()

link_directories(${OPENVR}/lib/${OPENVR_OS})
link_libraries(openvr_api)

include_directories(${OPENVR}/samples)
include_directories(include)

add_executable(hellovr_opengl src/hellovr_opengl_main.cpp src/hellovr_model.cpp src/hellovr_base.cpp ${EXTRASRC} ${OPENVR}/samples/shared/lodepng.cpp ${OPENVR}/samples/shared/Matrices.cpp src/pathtools.cpp)
target_link_libraries(hellovr_opengl p::glew ${EXTRALIB})

add_executable(tracked_camera src/tracked_camera.cpp src/lodepng.cpp ${OPENVR}/samples/shared/Matrices.cpp )
target_link_libraries(tracked_camera p::glew p::sdl2)
