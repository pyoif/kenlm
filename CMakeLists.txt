cmake_minimum_required(VERSION 3.28)

# Define a single cmake project
project(kenlm)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FORCE_STATIC "Build static executables" OFF)
option(COMPILE_TESTS "Compile tests" OFF)
option(ENABLE_PYTHON "Build Python bindings" OFF)
option(BUILD_PYTHON_STANDALONE "Build standalone C++ lib with Python install" OFF)
# Eigen3 less than 3.1.0 has a race condition: http://eigen.tuxfamily.org/bz/show_bug.cgi?id=466
# CMake CONFIG mode rejects major-version bumps (e.g. 5.x vs requested 3.x).
# Try exact version first, fall back to any Eigen3 package if 5.x is installed.
find_package(Eigen3 5.0 CONFIG)
include(CMakeDependentOption)
cmake_dependent_option(ENABLE_INTERPOLATE "Build interpolation program (depends on Eigen3)" ON "EIGEN3_FOUND AND NOT WIN32" OFF)

set(KENLM_MAX_ORDER 6 CACHE STRING "Maximum supported ngram order")

if (BUILD_PYTHON_STANDALONE)
  # Build a shared lib artifact in the same way the cpython setup.py builds
  # and return. That lib can be dlopened by other Python packages that need
  # cpp artifacts and can't use the bits compiled by setuptools Extension
  include(${CMAKE_CURRENT_LIST_DIR}/python/BuildStandalone.cmake)
  return()
endif()


if (FORCE_STATIC)
  #presumably overkill, is there a better way?
  #http://cmake.3232098.n2.nabble.com/Howto-compile-static-executable-td5580269.html
  set_property(GLOBAL PROPERTY LINK_SEARCH_START_STATIC ON)
  set_property(GLOBAL PROPERTY LINK_SEARCH_END_STATIC ON)
  set(BUILD_SHARED_LIBRARIES OFF)
  if (MSVC)
	set(flag_vars
	  CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
	  CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO
	  CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
	  CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
	  foreach(flag_var ${flag_vars})
	    if(${flag_var} MATCHES "/MD")
	      string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
	    endif(${flag_var} MATCHES "/MD")
	  endforeach(flag_var)
  else (MSVC)
    if (NOT CMAKE_C_COMPILER_ID MATCHES ".*Clang")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static")
    endif ()
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
  endif ()
  #Annoyingly the exectuables say "File not found" unless these are set
  set(CMAKE_EXE_LINK_DYNAMIC_C_FLAGS)
  set(CMAKE_EXE_LINK_DYNAMIC_CXX_FLAGS)
  set(CMAKE_SHARED_LIBRARY_C_FLAGS)
  set(CMAKE_SHARED_LIBRARY_CXX_FLAGS)
  set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS)
  set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS)
endif ()

# Compile all executables into bin/
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

# Compile all libraries into lib/
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if (COMPILE_TESTS)
  # Tell cmake that we want unit tests to be compiled
  include(CTest)
  enable_testing()
endif()

# Add our CMake helper functions
include(cmake/KenLMFunctions.cmake)

if(MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} /w34716")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /w34716")
endif()

# And our helper modules
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/modules)

# Vendored third-party headers
add_library(kenlm_cli11 INTERFACE)
target_include_directories(kenlm_cli11 INTERFACE ${CMAKE_SOURCE_DIR}/third_party/CLI11/include)
add_library(kenlm_doctest INTERFACE)
target_include_directories(kenlm_doctest INTERFACE ${CMAKE_SOURCE_DIR}/third_party/doctest/include)

include_directories(${CMAKE_SOURCE_DIR}/third_party/CLI11/include ${CMAKE_SOURCE_DIR}/third_party/doctest/include)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Process subdirectories
add_subdirectory(util)
add_subdirectory(lm)

if(ENABLE_PYTHON)
  add_subdirectory(python)
endif()

# Install targets
install(EXPORT kenlmTargets
  FILE kenlmTargets.cmake
  NAMESPACE kenlm::
  DESTINATION share/kenlm/cmake
)

foreach(SUBDIR IN ITEMS util util/double-conversion util/stream lm lm/builder lm/common lm/filter lm/interpolate)
  file(GLOB HEADERS ${CMAKE_CURRENT_LIST_DIR}/${SUBDIR}/*.h ${CMAKE_CURRENT_LIST_DIR}/${SUBDIR}/*.hh)
  install(FILES ${HEADERS} DESTINATION include/kenlm/${SUBDIR} COMPONENT headers)
endforeach(SUBDIR)

# Config
include(CMakePackageConfigHelpers)
# generate the config file that is includes the exports
configure_package_config_file(${PROJECT_SOURCE_DIR}/cmake/kenlmConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/kenlmConfig.cmake"
  INSTALL_DESTINATION share/kenlm/cmake
  NO_SET_AND_CHECK_MACRO
  NO_CHECK_REQUIRED_COMPONENTS_MACRO
  )
# install the configuration file
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/kenlmConfig.cmake
  DESTINATION share/kenlm/cmake
  )
