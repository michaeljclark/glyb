cmake_minimum_required(VERSION 3.10)

project(GraphStorm)

if (UNIX)
  find_package(PkgConfig)
  pkg_check_modules(GL REQUIRED gl)
  pkg_check_modules(GLFW3 REQUIRED glfw3)
  pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)
  pkg_check_modules(FREETYPE2 REQUIRED freetype2)
  add_definitions(${GLFW3_CFLAGS})
  add_definitions(${HARFBUZZ_CFLAGS})
  add_definitions(${FREETYPE2_CFLAGS})
  find_library(OPENGL_LIBRARY OpenGL)
  list(APPEND LIBS ${GLFW3_LDFLAGS} ${HARFBUZZ_LDFLAGS} ${FREETYPE2_LDFLAGS} ${OPENGL_LIBRARY})
  message(STATUS "OPENGL_LIBRARY = ${OPENGL_LIBRARY}")
endif()

include_directories(src)

set(BINPACK_SOURCES src/binpack.cc src/binpack.h src/font.cc src/font.h src/glyph.cc src/glyph.h src/text.cc src/text.h src/utf8.cc src/utf8.h src/util.cc src/util.h)
add_library(binpack STATIC ${BINPACK_SOURCES})

add_executable(glbinpack examples/glbinpack.cc)
target_link_libraries(glbinpack binpack ${LIBS})

add_executable(glfont examples/glfont.cc)
target_link_libraries(glfont binpack ${LIBS})

add_executable(gllayout examples/gllayout.cc)
target_link_libraries(gllayout binpack ${LIBS})

add_executable(glsimple examples/glsimple.cc)
target_link_libraries(glsimple binpack ${LIBS})

file(GLOB BINPACK_TESTS tests/*.cc)
foreach(src ${BINPACK_TESTS})
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src})
  target_link_libraries(${name} binpack ${LIBS})
endforeach(src)