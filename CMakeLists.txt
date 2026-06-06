cmake_minimum_required(VERSION 3.10)

file(STRINGS jbig2.h _major REGEX "^#define JBIG2_VERSION_MAJOR \\(([0-9]+)\\)$")
file(STRINGS jbig2.h _minor REGEX "^#define JBIG2_VERSION_MINOR \\(([0-9]+)\\)$")
string(REGEX REPLACE ".*MAJOR \\(([0-9]+)\\).*" "\\1" _ver_major "${_major}")
string(REGEX REPLACE ".*MINOR \\(([0-9]+)\\).*" "\\1" _ver_minor "${_minor}")

project(jbig2dec
  VERSION "${_ver_major}.${_ver_minor}"
  LANGUAGES C
)

option(JBIG2DEC_ENABLE_PNG "Enable PNG output support (requires libpng)" ON)

# System checks
include(CheckIncludeFile)
include(CheckFunctionExists)

check_include_file("getopt.h"          HAVE_GETOPT_H)
check_include_file("unistd.h"         HAVE_UNISTD_H)
check_include_file("strings.h"        HAVE_STRINGS_H)
check_include_file("stddef.h"         HAVE_STDDEF_H)
check_include_file("libintl.h"        HAVE_LIBINTL_H)
check_include_file("stdint.h"         HAVE_STDINT_H)
check_function_exists("getopt_long"   HAVE_GETOPT_LONG)
check_function_exists("memset"        HAVE_MEMSET)
check_function_exists("strdup"        HAVE_STRDUP)

# PNG support
set(PNG_SRCS)
set(PNG_LIBS)
if(JBIG2DEC_ENABLE_PNG)
  find_package(PNG QUIET)
  if(PNG_FOUND)
    set(PNG_SRCS jbig2_image_png.c)
    set(PNG_LIBS PNG::PNG)
  endif()
endif()

# Common compile definitions for all targets
# NOTE: we do NOT define HAVE_CONFIG_H. The source files use it to
# conditionally include config.h, but we pass all defines directly as
# compiler flags. os_types.h falls through to <stdint.h> (or config_win32.h
# on Windows) which works on all modern systems.
set(COMDEFS
  HAVE_STRING_H=1
)
if(MSVC)
  list(APPEND COMDEFS _CRT_SECURE_NO_WARNINGS)
endif()
foreach(_h getopt unistd strings stddef libintl stdint)
  if(HAVE_${_h}_H)
    list(APPEND COMDEFS "HAVE_${_h}_H=1")
  endif()
endforeach()
foreach(_f getopt_long memset strdup)
  if(HAVE_${_f})
    list(APPEND COMDEFS "HAVE_${_f}=1")
  endif()
endforeach()
if(PNG_FOUND)
  list(APPEND COMDEFS HAVE_LIBPNG=1)
endif()

# Extra source files for the executable only (not the library)
set(EXTRA_APP_SRCS)
if(NOT HAVE_GETOPT_LONG)
  list(APPEND EXTRA_APP_SRCS getopt.c getopt1.c)
endif()

# Library
set(LIB_SRCS
  jbig2.c
  jbig2_arith.c jbig2_arith_int.c jbig2_arith_iaid.c
  jbig2_huffman.c jbig2_hufftab.c
  jbig2_segment.c jbig2_page.c
  jbig2_symbol_dict.c jbig2_text.c
  jbig2_generic.c jbig2_refinement.c jbig2_mmr.c
  jbig2_halftone.c
  jbig2_image.c jbig2_image_pbm.c
  memento.c
  ${PNG_SRCS}
)

add_library(jbig2dec-lib ${LIB_SRCS})
target_compile_definitions(jbig2dec-lib PRIVATE ${COMDEFS})
target_include_directories(jbig2dec-lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(jbig2dec-lib PUBLIC ${PNG_LIBS})
set_target_properties(jbig2dec-lib PROPERTIES
  OUTPUT_NAME jbig2dec
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Executable
add_executable(jbig2dec
  jbig2dec.c sha1.c
  ${EXTRA_APP_SRCS}
)
target_link_libraries(jbig2dec jbig2dec-lib)
target_compile_definitions(jbig2dec PRIVATE ${COMDEFS})
target_include_directories(jbig2dec PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# Tests
add_executable(test_sha1 sha1.c)
target_compile_definitions(test_sha1 PRIVATE ${COMDEFS} TEST)
target_include_directories(test_sha1 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(test_huffman jbig2_huffman.c)
target_link_libraries(test_huffman jbig2dec-lib)
target_compile_definitions(test_huffman PRIVATE ${COMDEFS} TEST)
target_include_directories(test_huffman PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(test_arith jbig2_arith.c)
target_link_libraries(test_arith jbig2dec-lib)
target_compile_definitions(test_arith PRIVATE ${COMDEFS} TEST)
target_include_directories(test_arith PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# Install paths
include(GNUInstallDirs)

# pkg-config
set(prefix       "${CMAKE_INSTALL_PREFIX}")
set(exec_prefix  "\${prefix}")
set(libdir       "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
set(includedir   "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
set(VERSION      "${PROJECT_VERSION}")
configure_file(jbig2dec.pc.in jbig2dec.pc @ONLY)
install(TARGETS jbig2dec-lib jbig2dec
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES jbig2.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES "${CMAKE_BINARY_DIR}/jbig2dec.pc"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# Testing
enable_testing()
add_test(NAME test_sha1    COMMAND test_sha1)
add_test(NAME test_huffman COMMAND test_huffman)
add_test(NAME test_arith   COMMAND test_arith)
find_package(Python3 QUIET)
if(Python3_FOUND)
  configure_file(
    "${CMAKE_SOURCE_DIR}/test_jbig2dec.py"
    "${CMAKE_BINARY_DIR}/test_jbig2dec.py"
    COPYONLY
  )
  add_test(
    NAME test_jbig2dec_py
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_BINARY_DIR}/test_jbig2dec.py"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )
  set_tests_properties(test_jbig2dec_py PROPERTIES
    ENVIRONMENT "JBIG2DEC=$<TARGET_FILE:jbig2dec>"
  )
endif()
