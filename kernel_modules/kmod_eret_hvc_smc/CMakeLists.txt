# Builds the kernel module for speculation over ERET, HVC and SMC.

# Works only on ARM.
if(NOT "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "^aarch64$")
  message(STATUS "Skipping kernel module for speculation over ERET/HVC/SMC on non-aarch64 CPUs")
  return()
endif()

build_kernel_module(eret_hvc_smc_module)
