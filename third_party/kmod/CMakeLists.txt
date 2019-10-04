# Builds the Meltdown kernel module.

set(MODULE_NAME kernel_data)

# Determine what kernel source tree we'll use to build the module.
# CMAKE_SYSTEM_VERSION defaults to CMAKE_HOST_SYSTEM_VERSION, which is
# documented to use `uname -r` on systems (like Linux) that support `uname`.
# https://cmake.org/cmake/help/latest/variable/CMAKE_HOST_SYSTEM_VERSION.html
set(KERNEL_SRC "/lib/modules/${CMAKE_SYSTEM_VERSION}/build" )
message(STATUS "Kernel build directory: ${KERNEL_SRC}")

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
