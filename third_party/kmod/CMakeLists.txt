# build the kernel module
set(MODULE_NAME kernel_data)
set(MODULE_SOURCE_DIR ${PROJECT_SOURCE_DIR}/third_party/kmod)
set(MODULE_SOURCE_FILES ${PROJECT_SOURCE_DIR}/third_party/kmod/${MODULE_NAME}.c)
set(MODULE_FILE ${MODULE_NAME}.ko)

# CMAKE_SYSTEM_VERSION defaults to CMAKE_HOST_SYSTEM_VERSION, which is
# documented to use `uname -r` on systems (like Linux) that support `uname`.
# https://cmake.org/cmake/help/latest/variable/CMAKE_HOST_SYSTEM_VERSION.html
set(KERNEL_SRC "/lib/modules/${CMAKE_SYSTEM_VERSION}/build" )

message("selected kernel build directory ${KERNEL_SRC}")

message("generating module to ${MODULE_SOURCE_DIR}")

add_custom_command(
    OUTPUT ${MODULE_SOURCE_DIR}/${MODULE_FILE}
    COMMAND make ARGS -C ${KERNEL_SRC} M=${PROJECT_SOURCE_DIR}/third_party/kmod modules
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS ${MODULE_SOURCE_FILES}
    VERBATIM
)

add_custom_target(
    ${MODULE_NAME}
    ALL
    DEPENDS ${MODULE_SOURCE_DIR}/${MODULE_FILE}
)

# Help message for loading the module
add_custom_command(TARGET ${MODULE_NAME} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --green --bold 'use \"sudo insmod ${MODULE_SOURCE_DIR}/${MODULE_FILE}\" to insert the meltdown kernel module.')
