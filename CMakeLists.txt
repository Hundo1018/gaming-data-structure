cmake_minimum_required(VERSION 3.20)
project(game_ds_agent CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(GDS_NATIVE_ARCH "Compile for the host microarchitecture" ON)

include(CheckCXXCompilerFlag)
# -ffp-contract=off matters for the experiment, not for speed: with contraction
# allowed the compiler may fuse a multiply and an add in one candidate and not
# in another, and two structures would then disagree on a distance test at the
# boundary of a query radius. Identical float results across candidates are what
# makes their answers comparable at all.
set(GDS_FLAGS -O2 -DNDEBUG -fno-omit-frame-pointer -ffp-contract=off -Wall -Wextra)
if(GDS_NATIVE_ARCH)
  check_cxx_compiler_flag(-march=native GDS_HAS_MARCH_NATIVE)
  if(GDS_HAS_MARCH_NATIVE)
    list(APPEND GDS_FLAGS -march=native)
  endif()
endif()

add_library(gds_substrate STATIC
  substrate/src/alloc_tracker.cpp
  substrate/src/json.cpp
  substrate/src/pmu.cpp
  substrate/src/workload.cpp
  substrate/src/spatial_workload.cpp
)
target_include_directories(gds_substrate PUBLIC substrate/include)
target_compile_options(gds_substrate PUBLIC ${GDS_FLAGS})

# Every candidate directory that holds a structure.cpp becomes its own
# executable. A candidate that fails to compile removes only itself from the
# run, which is what lets a generated population be built without one bad
# member stopping the experiment.
file(GLOB GDS_CANDIDATE_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/candidates/*/*/structure.cpp")

set(GDS_CANDIDATE_TARGETS "")
foreach(src ${GDS_CANDIDATE_SOURCES})
  get_filename_component(dir "${src}" DIRECTORY)
  get_filename_component(cand_name "${dir}" NAME)
  get_filename_component(track_dir "${dir}" DIRECTORY)
  get_filename_component(track_name "${track_dir}" NAME)
  set(target "gds_${track_name}_${cand_name}")
  add_executable(${target} "${src}")
  # The candidate's own directory, and the candidates root so that a candidate
  # descended from another can include its parent by path rather than by copy.
  target_include_directories(${target} PRIVATE "${dir}" "${CMAKE_CURRENT_SOURCE_DIR}/candidates")
  target_link_libraries(${target} PRIVATE gds_substrate)
  list(APPEND GDS_CANDIDATE_TARGETS ${target})
endforeach()

message(STATUS "candidates: ${GDS_CANDIDATE_TARGETS}")
message(STATUS "flags: ${GDS_FLAGS}")

# Written so the orchestrator records the exact flags a measurement was taken
# under, rather than guessing them back out of the generator's files.
string(REPLACE ";" " " GDS_FLAGS_TEXT "${GDS_FLAGS}")
file(WRITE "${CMAKE_BINARY_DIR}/gds_build_info.txt"
     "flags=${GDS_FLAGS_TEXT}\n"
     "build_type=${CMAKE_BUILD_TYPE}\n"
     "compiler=${CMAKE_CXX_COMPILER}\n"
     "compiler_version=${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\n"
     "native_arch=${GDS_NATIVE_ARCH}\n")
