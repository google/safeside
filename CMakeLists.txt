cmake_minimum_required(VERSION 3.5.1)

project(safeside VERSION 0.1.0 LANGUAGES CXX)

add_library(safeside demos/cache_sidechannel.cc demos/instr.cc)

# Spectre V1 -- bounds check bypass
add_executable(spectre_v1 demos/spectre_v1.cc)
target_link_libraries(spectre_v1 safeside)

# Spectre V1 indirect jumps -- mistrating BTB instead of PHT.
add_executable(spectre_v1_indirect_jumps demos/spectre_v1_indirect_jumps.cc)
target_link_libraries(spectre_v1_indirect_jumps safeside)

# Spectre V4 -- speculative store bypass
add_executable(spectre_v4 demos/spectre_v4.cc)
target_link_libraries(spectre_v4 safeside)

if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  # Ret2Spec -- speculative execution using return stack buffers
  add_executable(ret2spec demos/ret2spec.cc)
  target_compile_options(ret2spec PRIVATE -fomit-frame-pointer)
  target_link_libraries(ret2spec safeside)
endif()


if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  add_subdirectory(third_party/kmod)

  # Spectre V3 / Meltdown
  add_executable(meltdown demos/meltdown.cc)
  target_link_libraries(meltdown safeside)
endif()
