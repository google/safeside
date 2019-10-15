# Builds the Meltdown kernel module.

# Check if we should skip building the module.
if(NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  message(STATUS "Skipping Meltdown kernel module on non-Linux OS")
  return()
elseif(NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "^(i.86)|(x86_64)|(ppc64le)$")
  message(STATUS "Skipping Meltdown kernel module on non-x86/64/ppc CPUs")
  return()
elseif(${CMAKE_CROSSCOMPILING})
  message(STATUS "Skipping Meltdown kernel module in cross-compile build")
  return()
endif()

build_kernel_module(meltdown_module)
