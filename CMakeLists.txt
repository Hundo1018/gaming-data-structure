cmake_minimum_required(VERSION 3.20)
project(game_ds_agent C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# GNU extensions stay off: the suite is C11 plus three compiler builtins
# (__builtin_ctzll, __builtin_clzll, __builtin_popcountll) and one empty asm
# barrier, which every GCC and Clang provides in strict mode too.
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(GDS_NATIVE_ARCH "Compile for the host microarchitecture" ON)

include(CheckCCompilerFlag)
# -ffp-contract=off matters for the experiment, not for speed: with contraction
# allowed the compiler may fuse a multiply and an add in one candidate and not
# in another, and two structures would then disagree on a distance test at the
# boundary of a query radius. Identical float results across candidates are what
# makes their answers comparable at all.
#
# -D_POSIX_C_SOURCE exposes clock_gettime, strtok_r and strdup, which strict
# C11 hides.
set(GDS_FLAGS -O2 -DNDEBUG -D_POSIX_C_SOURCE=200809L -fno-omit-frame-pointer
    -ffp-contract=off -Wall -Wextra -Werror=implicit-function-declaration
    -Werror=incompatible-pointer-types)
if(GDS_NATIVE_ARCH)
  check_c_compiler_flag(-march=native GDS_HAS_MARCH_NATIVE)
  if(GDS_HAS_MARCH_NATIVE)
    list(APPEND GDS_FLAGS -march=native)
  endif()
endif()

add_library(gds_substrate STATIC
  substrate/src/json.c
  substrate/src/measure.c
  substrate/src/pmu.c
  substrate/src/workload.c
  substrate/src/spatial_workload.c
)
target_include_directories(gds_substrate PUBLIC substrate/include)
target_compile_options(gds_substrate PUBLIC ${GDS_FLAGS})
target_link_libraries(gds_substrate PUBLIC m)

# The allocator defines malloc and its relatives, so the compiler must not
# treat the calls inside it as the builtins it is replacing. It is linked into
# every executable as an object, not from the archive: a static library member
# is only pulled in for a symbol nothing else defines, and libc defines malloc,
# so from an archive the counting allocator could silently lose to glibc's.
add_library(gds_alloc OBJECT substrate/src/alloc.c)
target_include_directories(gds_alloc PUBLIC substrate/include)
target_compile_options(gds_alloc PRIVATE ${GDS_FLAGS} -fno-builtin)

# Every candidate directory that holds a structure.c becomes its own
# executable. A candidate that fails to compile removes only itself from the
# run, which is what lets a generated population be built without one bad
# member stopping the experiment.
file(GLOB GDS_CANDIDATE_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/candidates/*/*/structure.c")

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
  target_sources(${target} PRIVATE $<TARGET_OBJECTS:gds_alloc>)
  target_link_libraries(${target} PRIVATE gds_substrate)
  list(APPEND GDS_CANDIDATE_TARGETS ${target})
endforeach()

# Research instruments: not candidates, never ranked. Each one measures
# something about the workloads themselves.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/substrate/tools/spatial_floor.c")
  add_executable(gds_floor_spatial substrate/tools/spatial_floor.c)
  target_include_directories(gds_floor_spatial PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/candidates")
  target_sources(gds_floor_spatial PRIVATE $<TARGET_OBJECTS:gds_alloc>)
  target_link_libraries(gds_floor_spatial PRIVATE gds_substrate)
endif()

message(STATUS "candidates: ${GDS_CANDIDATE_TARGETS}")
message(STATUS "flags: ${GDS_FLAGS}")

# Written so the orchestrator records the exact flags a measurement was taken
# under, rather than guessing them back out of the generator's files.
string(REPLACE ";" " " GDS_FLAGS_TEXT "${GDS_FLAGS}")
file(WRITE "${CMAKE_BINARY_DIR}/gds_build_info.txt"
     "flags=${GDS_FLAGS_TEXT}\n"
     "build_type=${CMAKE_BUILD_TYPE}\n"
     "compiler=${CMAKE_C_COMPILER}\n"
     "compiler_version=${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}\n"
     "native_arch=${GDS_NATIVE_ARCH}\n")
