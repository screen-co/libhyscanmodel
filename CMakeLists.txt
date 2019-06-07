
cmake_minimum_required (VERSION 2.8)

project (libhyscanmodel)

enable_testing()

set (HYSCAN_MAJOR_VERSION 5)
set (HYSCAN_MODEL_VERSION 0)
set (HYSCAN_MODEL_LIBRARY "hyscanmodel-${HYSCAN_MAJOR_VERSION}")

include (FindPkgConfig)

if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE Release)
endif ()

if (${CMAKE_C_COMPILER_ID} STREQUAL GNU OR ${CMAKE_C_COMPILER_ID} STREQUAL Clang)
  set (C_FLAGS "-Wall -Wextra -Wno-unused-parameter $ENV{CFLAGS}")
  set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 ${C_FLAGS} -Wmissing-prototypes")
  set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Og ${C_FLAGS}")
elseif (${CMAKE_C_COMPILER_ID} STREQUAL MSVC)
  set (C_FLAGS "/wd4244 $ENV{CFLAGS}")
  set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${C_FLAGS}")
  set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${C_FLAGS}")
else ()
  message (FATAL_ERROR "Unsupported compiler ${CMAKE_C_COMPILER_ID}")
endif ()

if (NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endif ()

if (NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endif ()

if (NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endif ()

pkg_check_modules (GLIB2 REQUIRED glib-2.0 gobject-2.0 gthread-2.0 gio-2.0)
pkg_check_modules (LIBXML2 REQUIRED libxml-2.0)
add_definitions (${GLIB2_CFLAGS} ${LIBXML2_CFLAGS})
link_directories (${GLIB2_LIBRARY_DIRS} ${LIBXML2_LIBRARY_DIRS})

if (HYSCAN_USE_SYS_LIBS)
  pkg_check_modules (HYSCAN REQUIRED "hyscancore-${HYSCAN_MAJOR_VERSION}")

  link_directories (${HYSCAN_LIBRARY_DIRS})
  add_definitions (${HYSCAN_CFLAGS})
else ()
  include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscantypes/hyscantypes")
  include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscancache/hyscancache")
  include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscanmath/hyscanmath")
  include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscandb/hyscandb")
  include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscandriver/hyscandriver")
  include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscancore/hyscancore")

  link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscantypes/bin")
  link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscancache/bin")
  link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscanmath/bin")
  link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscandb/bin")
  link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscandriver/bin")
  link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscancore/bin")

  set (HYSCAN_LIBRARIES "hyscantypes-${HYSCAN_MAJOR_VERSION}"
                        "hyscandriver-${HYSCAN_MAJOR_VERSION}"
                        "hyscanmath-${HYSCAN_MAJOR_VERSION}"
                        "hyscancache-${HYSCAN_MAJOR_VERSION}"
                        "hyscandb-${HYSCAN_MAJOR_VERSION}"
                        "hyscancore-${HYSCAN_MAJOR_VERSION}")
endif ()

include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/hyscanmodel")

add_definitions (-DG_LOG_DOMAIN="HyScanModel")
add_subdirectory (hyscanmodel)
add_subdirectory (tests)
