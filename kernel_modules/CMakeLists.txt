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
    message(WARNING
        " Kernel headers not found. Modules won't be built.\n"
        " Try:\n"
        "     sudo apt install linux-headers-${CMAKE_HOST_SYSTEM_VERSION}"
    )
    return()
  endif()

  # Create the directory where we'll invoke Kbuild to compile the module.
  #
  # So far as we can tell, Kbuild uses KBUILD_EXTMOD to set *both* the source
  # for the out-of-tree kernel module *and* the build output directory, so we
  # can't build directly from the source tree while putting outputs elsewhere.
  #
  # To avoid cluttering the tree, we *copy* Makefile and <module>.c into the
  # output directory and let Kbuild find them there.
  set(MODULE_OUTPUT_DIR ${CMAKE_BINARY_DIR}/${MODULE_NAME})
  file(MAKE_DIRECTORY ${MODULE_OUTPUT_DIR})

  # Get a friendly relative path to the kernel module.
  file(
      RELATIVE_PATH
      MODULE_RELATIVE_PATH
      ${PROJECT_SOURCE_DIR}
      ${MODULE_OUTPUT_DIR}/${MODULE_NAME}.ko
  )

  # Create the actual build rule for the kernel module target.
  add_custom_command(
      OUTPUT ${MODULE_OUTPUT_DIR}/${MODULE_NAME}.ko

      # Copy the Makefile and module implementation into the output directory.
      #
      # We do this here instead of using FILE(COPY ...) above because the FILE
      # command only executes once at configuration time, and won't update
      # these copies when the originals change.
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_CURRENT_SOURCE_DIR}/Makefile
          ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_NAME}.c
          ${MODULE_OUTPUT_DIR}

      # Invoke Kbuild to build the module in the output directory.
      # https://www.kernel.org/doc/Documentation/kbuild/modules.txt
      COMMAND make
          -C ${KERNEL_SRC}
          KBUILD_EXTMOD=${MODULE_OUTPUT_DIR}
          modules

      # Show help for installing the newly-built module.
      COMMAND ${CMAKE_COMMAND} -E cmake_echo_color
          ""
          --green --bold
          "To install the kernel module, run:"
          --normal
          "    sudo insmod ${MODULE_RELATIVE_PATH}"
          ""

      DEPENDS
          Makefile
          ${MODULE_NAME}.c
      VERBATIM
  )

  # Create a target to serve as a friendly name for the output module.
  add_custom_target(
      ${MODULE_NAME}
      ALL
      DEPENDS ${MODULE_OUTPUT_DIR}/${MODULE_NAME}.ko
  )
endfunction(build_kernel_module)

add_subdirectory(kmod_eret_hvc_smc)
add_subdirectory(kmod_meltdown)
