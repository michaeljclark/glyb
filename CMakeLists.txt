cmake_minimum_required(VERSION 3.10)

project(glyb)

#
# project settings
#

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

#
# compiler flags
#

include(CheckCXXCompilerFlag)

if(MSVC)
  add_definitions(/MP)
  add_definitions(/D_SECURE_SCL=0 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS)
endif()

check_cxx_compiler_flag("-pg" has_gprof "int main() { return 0; }")
if (CMAKE_PROFILE AND has_gprof)
  add_compile_options(-pg)
endif()

check_cxx_compiler_flag("-fno-omit-frame-pointer" has_no_omit_fp "int main() { return 0; }")
if (has_no_omit_fp)
  add_compile_options(-fno-omit-frame-pointer)
endif()

#
# user options
#

option(GLYB_BUILD_TOOLS "Build glyb tools" ON)
option(GLYB_BUILD_EXAMPLES "Build glyb examples" ON)
option(GLYB_BUILD_TESTS "Build glyb tests" ON)

#
# system libraries
#

find_package(Threads REQUIRED)
list(APPEND GLYB_LIBS Threads::Threads)

#
# bundled libraries
#

## glad
add_subdirectory(third_party/glad)
include_directories(${CMAKE_BINARY_DIR}/third_party/glad/include)

## glfw
add_subdirectory(third_party/glfw)
include_directories(third_party/glfw/include)

# brotli
set(BROTLI_BUNDLED_MODE OFF CACHE BOOL "")
add_subdirectory(third_party/brotli)
include_directories(third_party/brotli/c/include)
set_property(TARGET brotlidec-static PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
set_property(TARGET brotlicommon-static PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
install(TARGETS brotlidec-static brotlicommon-static EXPORT brotli-targets DESTINATION "${INSTALL_BIN_DIR}")
install(EXPORT brotli-targets DESTINATION lib)

## bzip2
set(ENABLE_LIB_ONLY ON CACHE BOOL "")
set(ENABLE_SHARED_LIB OFF CACHE BOOL "")
set(ENABLE_STATIC_LIB ON CACHE BOOL "")
add_subdirectory(third_party/bzip2)
include_directories(third_party/bzip2)

## zlib
add_subdirectory(third_party/zlib)
include_directories(third_party/zlib)
include_directories(${CMAKE_BINARY_DIR}/third_party/zlib)
install(TARGETS zlib zlibstatic EXPORT zlib-targets DESTINATION "${INSTALL_BIN_DIR}")
install(EXPORT zlib-targets DESTINATION lib)

## png
add_subdirectory(third_party/libpng)
include_directories(third_party/libpng)
include_directories(${CMAKE_BINARY_DIR}/third_party/libpng)

## freetype2
add_subdirectory(third_party/freetype2)
add_definitions( -DFT_CONFIG_CONFIG_H=\"ftconfig.h\" )
add_definitions( -DFT_CONFIG_MODULES_H=\"ftmodule.h\" )
include_directories(third_party/freetype2/include)
include_directories(third_party/freetype2/include/freetype/config)

## harfbuzz
set(HB_HAVE_FREETYPE ON CACHE BOOL "Enable FreeType Integration" FORCE)
add_subdirectory(third_party/harfbuzz)
include_directories(third_party/harfbuzz/src)

## imgui
set(IMGUI_SOURCES
  third_party/imgui/backends/imgui_impl_glfw.cpp
  third_party/imgui/backends/imgui_impl_opengl3.cpp
  third_party/imgui/imgui.cpp
  third_party/imgui/imgui_demo.cpp
  third_party/imgui/imgui_draw.cpp
  third_party/imgui/imgui_tables.cpp
  third_party/imgui/imgui_widgets.cpp)
include_directories(third_party/imgui)
include_directories(third_party/imgui/backends)
add_definitions(-DIMGUI_IMPL_OPENGL_LOADER_GLAD)
add_library(imgui STATIC ${IMGUI_SOURCES})
add_dependencies(imgui glad-generate-files)

## msdfgen
add_subdirectory(third_party/msdfgen)
include_directories(third_party/msdfgen)

## glm
include_directories(third_party/glm)

#
# libglyb
#

include_directories(src)
include_directories(examples)
file(GLOB GLYB_SOURCES src/*.cc src/*.h)
add_library(glyb STATIC ${GLYB_SOURCES})
add_dependencies(glyb glad-generate-files)
set_target_properties(glyb PROPERTIES CXX_STANDARD 17)
list(APPEND GLYB_LIBS
  ${PNG_LIBRARY}
  ${BROTLIDEC_LIBRARY}
  ${BZIP_LIBRARY}
  ${ZLIB_LIBRARY}
  ${HARFBUZZ_LIBRARY}
  ${FREETYPE_LIBRARY}
  glyb
  lib_msdfgen)

#
# CLI programs
#

if(GLYB_BUILD_TOOLS)
  foreach(prog IN ITEMS bitcode ftkitty ftrender fontdb fontscan genatlas uniscan)
    add_executable(${prog} examples/${prog}.cc)
    target_link_libraries(${prog} ${GLYB_LIBS} ${CMAKE_DL_LIBS})
  endforeach(prog)
endif()

#
# OpenGL examples
#

if(GLYB_BUILD_EXAMPLES)
  foreach(prog IN ITEMS glbinpack glemoji glgraph glglyph glfont gllayout glsimple gldemo)
    add_executable(${prog} examples/${prog}.cc)
    target_link_libraries(${prog} ${GLYB_LIBS} ${CMAKE_DL_LIBS} ${GLAD_LIBRARIES} glfw)
    target_compile_definitions(${prog} PRIVATE -DHAVE_GLAD)
    if(WIN32)
      set_property(TARGET ${prog} PROPERTY WIN32_EXECUTABLE TRUE)
    endif()
  endforeach(prog)

  add_executable(glcanvas examples/glcanvas.cc)
  target_link_libraries(glcanvas ${GLYB_LIBS} ${CMAKE_DL_LIBS} ${GLAD_LIBRARIES} glfw imgui)
  target_compile_definitions(glcanvas PRIVATE -DHAVE_GLAD)
  if(WIN32)
    set_property(TARGET glcanvas PROPERTY WIN32_EXECUTABLE TRUE)
  endif()
endif()

#
# glyb tests
#

if(GLYB_BUILD_TESTS)
  file(GLOB BINPACK_TESTS tests/*.cc)
  foreach(src ${BINPACK_TESTS})
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src})
    target_link_libraries(${name} ${GLYB_LIBS})
  endforeach(src)
endif()
