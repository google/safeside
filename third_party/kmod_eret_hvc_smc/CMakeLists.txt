# Builds the kernel module for speculation over ERET, HVC and SMC.

# Check if we should skip building the module.
if(NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  message(STATUS "Skipping kernel module for speculation over ERET/HVC/SMC on non-Linux OS")
  return()
elseif(NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "^aarch64$")
  message(STATUS "Skipping kernel module for speculation over ERET/HVC/SMC on non-aarch64 CPUs")
  return()
elseif(${CMAKE_CROSSCOMPILING})
  message(STATUS "Skipping kernel module for speculation over ERET/HVC/SMC in cross-compile build")
  return()
endif()

build_kernel_module(eret_hvc_smc_module)
