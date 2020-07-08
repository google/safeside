# Enable (or at least don't disable) some optimizations in all builds.
# Without optimizations, it can be difficult to fit enough useful code into the
# speculative execution window.
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  # Remove a few optimization-related MSVC flags that CMake includes in Debug
  # builds by default: https://git.io/Jv0F0

  # Remove `/Od`, which disables all optimizations.
  # https://docs.microsoft.com/en-us/cpp/build/reference/od-disable-debug?view=vs-2019
  string(REPLACE "/Od" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

  # Remove `/Ob0`, which disables *all* function inlining -- even for functions
  # marked `__forceinline`.
  # https://docs.microsoft.com/en-us/cpp/build/reference/ob-inline-function-expansion?view=vs-2019
  string(REPLACE "/Ob0" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

  # Remove `/RTC1`, which enables run-time error checks. These checks are
  # incompatible with optimizations and the build fails if both are enabled.
  # https://docs.microsoft.com/en-us/cpp/build/reference/rtc-run-time-error-checks?view=vs-2019
  string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
else()
  # Enable baseline optimizations (`-O1`) in the generic C++ build flags for
  # all build types.
  #
  # Some experiments show that flags in `CMAKE_CXX_FLAGS` appear on the
  # compiler command line before those in `CMAKE_CXX_FLAGS_<CONFIG>` or added
  # with `add_compile_options()`. Since Clang and GCC use the last `-O` option,
  # that means we won't clobber a higher optimization setting used by release
  # builds. For example, `CMAKE_<LANG>_FLAGS_RELEASE` includes `-O3` by default
  # when compiling with a GCC-compatible toolchain (see https://git.io/Jv0xu).
  string(APPEND CMAKE_CXX_FLAGS " -O1")
endif()

# When targeting x86, we need to opt in to SSE2 instructions like
# clflush, mfence, lfence.
if(("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "^(i.86)$") AND
   ("${CMAKE_C_COMPILER_ID}" MATCHES "^(Clang)|(GNU)$"))
  add_compile_options(-msse2)
endif()

# Support library
add_library(safeside
    cache_sidechannel.cc
    instr.cc
    timing_array.cc
    utils.cc
)

if(UNIX)
  target_sources(safeside PRIVATE faults.cc)
endif()

# Configure the assembler. Set ASM_EXT (extension for assembly files) and
# ASM_PLATFORM (target CPU), which we'll use to add the right assembly
# implementation.

enable_language(ASM)
if("${CMAKE_ASM_COMPILER_ID}" STREQUAL "MSVC")
  set(ASM_EXT asm)

  # MSVC uses the MASM assembler.
  enable_language(ASM_MASM)
else()
  set(ASM_EXT S)
endif()

if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "^(i[3456]86)|(x86_64)|(x86)|(AMD64)$")
  # Flatten all these names down to x86 or x86_64.

  # On Windows the processor string is "AMD64" even if we're compiling for
  # 32-bit, so instead we rely on the compiler's view of the world.
  if("${CMAKE_SIZEOF_VOID_P}" EQUAL 4)
    set(ASM_PLATFORM x86)

    # Enable /SAFESEH for MASM on x86. Avoids a LNK2026 error when linking an
    # assembly library into a binary, since MSVC wants to link the binary with
    # SAFESEH and that requires all linked-in libraries be compatible.
    set(CMAKE_ASM_MASM_FLAGS "${CMAKE_ASM_MASM_FLAGS} /safeseh")
  else()
    set(ASM_PLATFORM x86_64)
  endif()
else()
  # For other platforms we can just trust CMake's value.
  set(ASM_PLATFORM ${CMAKE_SYSTEM_PROCESSOR})
endif()

# Compile the assembly implementation into to the support library.
target_sources(
  safeside
  PRIVATE
    asm/measurereadlatency_${ASM_PLATFORM}.${ASM_EXT}
)

# Support library tests

add_executable(timing_array_test timing_array_test.cc)
target_link_libraries(timing_array_test safeside)

if(UNIX)
  add_executable(faults_test faults_test.cc)
  target_link_libraries(faults_test safeside)
endif()

# Defines an executable target named `demo_name` built from `demo_name.cc` and
# linked against the Safeside support library. The caller can also use the
# SYSTEMS and PROCESSORS keywords to restrict when the target should be
# created.
function(add_demo demo_name)
  cmake_parse_arguments(
      ARG  # parsed argument prefix
      ""  # boolean options
      ""  # one-value arguments
      "SYSTEMS;PROCESSORS;ADDITIONAL_SOURCES"  # multi-value arguments
      ${ARGN}  # arguments to parse -- ARGN excludes already-named arguments
  )

  if (DEFINED ARG_SYSTEMS)
    if (NOT "${CMAKE_SYSTEM_NAME}" IN_LIST ARG_SYSTEMS)
      return()
    endif()
  endif()

  if (DEFINED ARG_PROCESSORS)
    if (NOT "${CMAKE_SYSTEM_PROCESSOR}" IN_LIST ARG_PROCESSORS)
      return()
    endif()
  endif()

  add_executable(${demo_name} ${demo_name}.cc ${ARG_ADDITIONAL_SOURCES})
  target_link_libraries(${demo_name} safeside)
endfunction()

# Spectre V1 PHT SA -- mistraining PHT in the same address space
add_demo(spectre_v1_pht_sa)

# Spectre V1 BTB SA -- mistraining BTB in the same address space
add_demo(spectre_v1_btb_sa)

# Spectre V4 -- speculative store bypass
add_demo(spectre_v4)

# Ret2Spec -- rewriting the RSB using recursion in the same address space
add_demo(ret2spec_sa ADDITIONAL_SOURCES ret2spec_common.cc)

# Spectre V1 BTB CA - mistraining BTB from another address space
add_demo(spectre_v1_btb_ca SYSTEMS Linux)

# Ret2spec CA -- rewriting the RSB using recursion from another address space
add_demo(ret2spec_ca SYSTEMS Linux ADDITIONAL_SOURCES ret2spec_common.cc)

# Ret2Spec -- speculative execution using return stack buffers creating a
# call-ret disparity by inline assembly
add_demo(ret2spec_callret_disparity
         SYSTEMS Linux Darwin)
if (TARGET ret2spec_callret_disparity)
  target_compile_options(ret2spec_callret_disparity
                         PRIVATE -fomit-frame-pointer)
endif()

# Spectre V3 / Meltdown
add_demo(meltdown SYSTEMS Linux PROCESSORS i686 x86_64 ppc64le)

# L1 terminal fault -- Foreshadow OS -- Meltdown P
add_demo(l1tf SYSTEMS Linux PROCESSORS i686 x86_64 ppc64le)

# Speculation over ERET, HVC and SMC instructions
add_demo(eret_hvc_smc_wrapper SYSTEMS Linux PROCESSORS aarch64)

# Speculation over syscall
add_demo(speculation_over_syscall SYSTEMS Linux PROCESSORS aarch64)

# Meltdown UD -- speculation over an undefined instruction
add_demo(meltdown_ud SYSTEMS Linux PROCESSORS aarch64)

# Meltdown BR - speculation over the ia32 bounds check instruction
add_demo(meltdown_br SYSTEMS Linux Darwin PROCESSORS i686)

# Meltdown SS -- speculative reading from non present segments and outside of
# segment limits
add_demo(meltdown_ss SYSTEMS Linux PROCESSORS i686)

# Meltdown OF -- speculative fetching from an overflowing address after an
# INTO check
add_demo(meltdown_of SYSTEMS Linux Darwin PROCESSORS i686)

# Speculation over hardware breakpoint trap (read watcher)
add_demo(speculation_over_read_hw_breakpoint
         SYSTEMS Linux
         PROCESSORS i686 x86_64 ppc64le)

# Speculation over hardware breakpoint fault (execution watcher)
add_demo(speculation_over_exec_hw_breakpoint
         SYSTEMS Linux
         PROCESSORS i686 x86_64)

# Speculation over single-step debug trap (trap flag in EFLAGS)
add_demo(speculation_over_single_step_trap SYSTEMS Linux PROCESSORS i686 x86_64)

# Meltdown AC -- speculative fetching of unaligned data
add_demo(meltdown_ac SYSTEMS Linux PROCESSORS i686 x86_64)

# Meltdown DE -- speculative computation with division by zero remainder
add_demo(meltdown_de SYSTEMS Linux Darwin PROCESSORS i686 x86_64)
