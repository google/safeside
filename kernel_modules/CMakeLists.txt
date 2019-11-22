function(build_kernel_module MODULE_NAME)
# Check if we should skip building the module.
if(NOT "${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
  message(STATUS "Skipping Linux kernel module on non-Linux OS")
  return()
elseif("${CMAKE_HOST_SYSTEM_VERSION}" MATCHES "-Microsoft$")
  message(STATUS "Skipping Linux kernel module in WSL")
  return()
elseif("${CMAKE_CROSSCOMPILING}")
  message(STATUS "Skipping kernel module in cross-compile build")
  return()
endif()
# Determine what kernel source tree we'll use to build the module.
# CMAKE_HOST_SYSTEM_VERSION uses `uname -r` on Linux.
# https://cmake.org/cmake/help/latest/variable/CMAKE_HOST_SYSTEM_VERSION.html
set(KERNEL_SRC "/lib/modules/${CMAKE_HOST_SYSTEM_VERSION}/build")

if(EXISTS "${KERNEL_SRC}")
  message(STATUS "Kernel build directory: ${KERNEL_SRC}")
else()
  message(FATAL_ERROR
      " Can't find headers to build Meltdown kernel module.\n"
      " Try:\n"
      "     sudo apt install linux-headers-${CMAKE_HOST_SYSTEM_VERSION}"
  )
endif()

# Move the inputs into a directory in the output first. Kbuild seems to use
# KBUILD_EXTMOD to set both the source of the out-of-tree kernel module to
# build *and* the build output directory, so we can't build from the source
# tree while putting outputs somewhere else.
set(MODULE_OUTPUT_DIR ${CMAKE_BINARY_DIR}/${MODULE_NAME})
file(MAKE_DIRECTORY ${MODULE_OUTPUT_DIR})
file(COPY Makefile ${MODULE_NAME}.c DESTINATION ${MODULE_OUTPUT_DIR})

# Invoke the kernel Makefile (kbuild) to build the kernel module.
# https://www.kernel.org/doc/Documentation/kbuild/modules.txt
add_custom_command(
    OUTPUT ${MODULE_OUTPUT_DIR}/${MODULE_NAME}.ko
    COMMAND make
        -C ${KERNEL_SRC}
        KBUILD_EXTMOD=${MODULE_OUTPUT_DIR}
        modules
    DEPENDS
        ${MODULE_OUTPUT_DIR}/Makefile
        ${MODULE_OUTPUT_DIR}/${MODULE_NAME}.c
    VERBATIM
)

# Create a target to run the custom command.
add_custom_target(
    ${MODULE_NAME}
    ALL
    DEPENDS ${MODULE_OUTPUT_DIR}/${MODULE_NAME}.ko
)

# Get a friendly relative path to the kernel module.
file(
    RELATIVE_PATH
    MODULE_RELATIVE_PATH
    ${PROJECT_SOURCE_DIR}
    ${MODULE_OUTPUT_DIR}/${MODULE_NAME}.ko
)

# After building, show help for installing the kernel module.
add_custom_command(
    TARGET ${MODULE_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    ARGS
        -E cmake_echo_color
        ""
        --green --bold
        "To install the Meltdown kernel module, run:"
        --normal
        "    sudo insmod ${MODULE_RELATIVE_PATH}"
        ""
    VERBATIM
)
endfunction(build_kernel_module)

add_subdirectory(kmod_eret_hvc_smc)
add_subdirectory(kmod_meltdown)
