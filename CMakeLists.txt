cmake_minimum_required(VERSION 3.10)

project(binpack)

if(UNIX AND NOT APPLE)
  set(LINUX true)
endif()

if(LINUX)
  find_package(PkgConfig)
  pkg_check_modules(GLFW3 REQUIRED glfw3)
  pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)
  pkg_check_modules(FREETYPE2 REQUIRED freetype2)
  add_definitions(${GLFW3_CFLAGS})
  add_definitions(${HARFBUZZ_CFLAGS})
  add_definitions(${FREETYPE2_CFLAGS})
  find_library(OPENGL_LIBRARY GL)
  list(APPEND LIBS ${HARFBUZZ_LDFLAGS} ${FREETYPE2_LDFLAGS} ${GLFW3_LDFLAGS} ${OPENGL_LIBRARY})
endif()

if(APPLE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -std=c99")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")
  find_library(SYSTEM_CONFIGURATION_LIBRARY SystemConfiguration)
  find_library(COREFOUNDATION_LIBRARY CoreFoundation)
  find_library(COCOA_LIBRARY Cocoa)
  find_library(GLUT_LIBRARY GLUT)
  find_library(AGL_LIBRARY AGL)
  find_library(OPENGL_LIBRARY OpenGL)
  list(APPEND LIBS ${SYSTEM_CONFIGURATION_LIBRARY} ${COREFOUNDATION_LIBRARY} ${COCOA_LIBRARY} ${OPENGL_LIBRARY})
  set(USE_BUNDLED_LIBS true)
endif()

if(WIN32)
  add_definitions("/MP")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_INIT} /D _SECURE_SCL=0 /D _WINDOWS /D _CRT_SECURE_NO_WARNINGS")
  find_library(OPENGL_LIBRARY OpenGL32)
  list(APPEND LIBS ${OPENGL_LIBRARY})
  set(USE_BUNDLED_LIBS true)
endif()

add_subdirectory(third_party/glad)

if(USE_BUNDLED_LIBS)
  add_subdirectory(third_party/glfw)
  add_subdirectory(third_party/freetype2)
  add_subdirectory(third_party/harfbuzz)
  include_directories(third_party/glfw/include)
  include_directories(third_party/freetype2/include)
  include_directories(third_party/freetype2/include/freetype/config)
  include_directories(third_party/harfbuzz/src)
  add_definitions( -DFT_CONFIG_CONFIG_H=\"ftconfig.h\" )
  add_definitions( -DFT_CONFIG_MODULES_H=\"ftmodule.h\" )
  list(INSERT LIBS 0 harfbuzz freetype glfw)
endif()

include_directories(src)

set(BINPACK_SOURCES
  src/binpack.cc
  src/binpack.h
  src/font.cc
  src/font.h
  src/glyph.cc
  src/glyph.h
  src/text.cc
  src/text.h
  src/utf8.cc
  src/utf8.h
  src/util.cc
  src/util.h
)
add_library(binpack STATIC ${BINPACK_SOURCES})

add_executable(glbinpack examples/glbinpack.cc)
target_link_libraries(glbinpack binpack ${LIBS} glad ${CMAKE_DL_LIBS})

add_executable(glfont examples/glfont.cc)
target_link_libraries(glfont binpack ${LIBS} glad ${CMAKE_DL_LIBS})

add_executable(gllayout examples/gllayout.cc)
target_link_libraries(gllayout binpack ${LIBS} glad ${CMAKE_DL_LIBS})

add_executable(glsimple examples/glsimple.cc)
target_link_libraries(glsimple binpack ${LIBS} glad ${CMAKE_DL_LIBS})

file(GLOB BINPACK_TESTS tests/*.cc)
foreach(src ${BINPACK_TESTS})
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src})
  target_link_libraries(${name} binpack ${LIBS})
endforeach(src)