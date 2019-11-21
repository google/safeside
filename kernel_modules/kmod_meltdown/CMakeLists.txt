# Builds the Meltdown kernel module.

# Works only on x86/64 and PowerPC.
if(NOT "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "^(i.86)|(x86_64)|(ppc64le)$")
  message(STATUS "Skipping Meltdown kernel module on non-x86/64/ppc CPUs")
  return()
endif()

build_kernel_module(meltdown_module)
