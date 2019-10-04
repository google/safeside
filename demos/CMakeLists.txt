# Use C++11 without extensions
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS off)

# Enable at least some optimization in all builds. The Ret2Spec demo, in
# particular, will segfault if built without optimizations.
# TODO(https://git.io/JecmX): Fix the crash in Ret2Spec
add_compile_options(-O1)

# Disable run-time code checking on MSVC. It's enabled by default in CMake's
# debug build settings, but it's incompatible with optimizations.
string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})

# Support library
add_library(safeside cache_sidechannel.cc instr.cc)

# Spectre V1 -- bounds check bypass
add_executable(spectre_v1 spectre_v1.cc)
target_link_libraries(spectre_v1 safeside)

# Spectre V1 indirect jumps -- mistrating BTB instead of PHT.
add_executable(spectre_v1_indirect_jumps spectre_v1_indirect_jumps.cc)
target_link_libraries(spectre_v1_indirect_jumps safeside)

# Spectre V4 -- speculative store bypass
add_executable(spectre_v4 spectre_v4.cc)
target_link_libraries(spectre_v4 safeside)

if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  # Ret2Spec -- speculative execution using return stack buffers
  add_executable(ret2spec ret2spec.cc)
  target_compile_options(ret2spec PRIVATE -fomit-frame-pointer)
  target_link_libraries(ret2spec safeside)
endif()

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  add_subdirectory(../third_party/kmod kmod)

  # Spectre V3 / Meltdown
  add_executable(meltdown meltdown.cc)
  target_link_libraries(meltdown safeside)
endif()
