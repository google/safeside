set(CMAKE_SYSTEM_NAME Linux)

# Linux and GNU have different names for little-endian 64-bit PowerPC.
#
# Linux calls it "ppc64le"[1], and that's what CMake ends up using for
# CMAKE_SYSTEM_PROCESSOR since it's set using `uname -r`[2] which comes from
# the kernel.
#
# GNU calls it "powerpc64le"[3], so we use that to name this toolchain. It's
# also the fragment that appears in the toolchain binary names and the name of
# the package we used to install the cross-compile toolchain[4].
#
# [1] https://git.io/JeahC
# [2] https://git.io/Jeahc
# [3] https://git.io/Jeaho
# [4] https://packages.debian.org/stable/gcc-powerpc64le-linux-gnu
set(CMAKE_SYSTEM_PROCESSOR ppc64le)

set(CMAKE_C_COMPILER powerpc64le-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER powerpc64le-linux-gnu-g++)
