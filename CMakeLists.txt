cmake_minimum_required(VERSION 3.10)

project(glyphic)

if(UNIX AND NOT APPLE)
  set(LINUX true)
endif()

if(LINUX)
  find_package(PkgConfig)
  find_library(OPENGL_LIBRARY GL)
  list(APPEND LIBS ${OPENGL_LIBRARY})
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

find_package(Threads REQUIRED)
list(APPEND LIBS Threads::Threads)

include_directories(third_party/include)

add_subdirectory(third_party/glad)

if(USE_BUNDLED_LIBS)
  add_subdirectory(third_party/glfw)
  include_directories(third_party/glfw/include)

  add_subdirectory(third_party/freetype2)
  add_definitions( -DFT_CONFIG_CONFIG_H=\"ftconfig.h\" )
  add_definitions( -DFT_CONFIG_MODULES_H=\"ftmodule.h\" )
  include_directories(third_party/freetype2/include)
  include_directories(third_party/freetype2/include/freetype/config)

  add_subdirectory(third_party/zlib)
  include_directories(third_party/zlib)

  # fixme: PNG_BUILD_ZLIB appears to not work so we copy headers
  set(PNG_BUILD_ZLIB third_party/zlib CACHE PATH "Enable custom Zlib" FORCE)
  set(ZLIB_LIBRARY zlibstatic)
  set(ZLIB_INCLUDE_DIR third_party/zlib)
  add_subdirectory(third_party/libpng)
  include_directories(third_party/libpng)
  target_include_directories(png PRIVATE third_party/zlib)

  # this glorius hack is required so that libpng finds the included zlib
  # problem is due to generated files not seeing include_directories()
  add_custom_target(png_zconf_h
    COMMAND ${CMAKE_COMMAND} -E copy
    ${PROJECT_BINARY_DIR}/third_party/zlib/zconf.h
    ${PROJECT_BINARY_DIR}/third_party/libpng/.)
  add_custom_target(png_zlib_h
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_SOURCE_DIR}/third_party/zlib/zlib.h
    ${PROJECT_BINARY_DIR}/third_party/libpng/.)
  add_dependencies(genfiles png_zlib_h png_zconf_h)

  set(HB_HAVE_FREETYPE ON CACHE BOOL "Enable FreeType Integration" FORCE)
  set(FREETYPE_INCLUDE_DIRS third_party/freetype2/include)
  set(FREETYPE_LIBRARY freetype)
  add_subdirectory(third_party/harfbuzz)
  include_directories(third_party/harfbuzz/src)
  list(INSERT LIBS 0 png_static zlibstatic harfbuzz freetype glfw)
else()
  pkg_check_modules(GLFW3 REQUIRED glfw3)
  pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)
  pkg_check_modules(FREETYPE2 REQUIRED freetype2)
  pkg_check_modules(PNG REQUIRED libpng)
  add_definitions(${GLFW3_CFLAGS})
  add_definitions(${HARFBUZZ_CFLAGS})
  add_definitions(${FREETYPE2_CFLAGS})
  add_definitions(${PNG_CFLAGS})
  list(INSERT LIBS 0 ${HARFBUZZ_LDFLAGS} ${FREETYPE2_LDFLAGS} ${GLFW3_LDFLAGS} ${PNG_LDFLAGS})
endif()

add_subdirectory(third_party/msdfgen)
include_directories(third_party/msdfgen)
list(APPEND LIBS lib_msdfgen)

include_directories(src)

set(GLYPHIC_SOURCES
  src/binpack.cc
  src/binpack.h
  src/file.cc
  src/file.h
  src/font.cc
  src/font.h
  src/glyph.cc
  src/glyph.h
  src/image.cc
  src/image.h
  src/logger.cc
  src/logger.h
  src/msdf.cc
  src/msdf.h
  src/multi.cc
  src/multi.h
  src/text.cc
  src/text.h
  src/utf8.cc
  src/utf8.h
  src/util.cc
  src/util.h
)
add_library(glyphic STATIC ${GLYPHIC_SOURCES})

add_executable(genatlas examples/genatlas.cc)
target_link_libraries(genatlas glyphic ${LIBS} ${CMAKE_DL_LIBS})

add_executable(glbinpack examples/glbinpack.cc)
target_link_libraries(glbinpack glyphic ${LIBS} glad ${CMAKE_DL_LIBS})

add_executable(glfont examples/glfont.cc)
target_link_libraries(glfont glyphic ${LIBS} glad ${CMAKE_DL_LIBS})

add_executable(gllayout examples/gllayout.cc)
target_link_libraries(gllayout glyphic ${LIBS} glad ${CMAKE_DL_LIBS})

add_executable(glsimple examples/glsimple.cc)
target_link_libraries(glsimple glyphic ${LIBS} glad ${CMAKE_DL_LIBS})

file(GLOB BINPACK_TESTS tests/*.cc)
foreach(src ${BINPACK_TESTS})
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src})
  target_link_libraries(${name} glyphic ${LIBS})
endforeach(src)