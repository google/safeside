# Use C++11 without extensions
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS off)

# Enable at least some optimization in all builds. The Ret2Spec demo, in
# particular, will segfault if built without optimizations.
# TODO(https://git.io/JecmX): Fix the crash in Ret2Spec
add_compile_options(-O2)

# Disable run-time code checking on MSVC. It's enabled by default in CMake's
# debug build settings, but it's incompatible with optimizations.
string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})

# When targeting x86, we need to opt in to SSE2 instructions like
# clflush, mfence, lfence.
if((${CMAKE_SYSTEM_PROCESSOR} MATCHES "^(i.86)$") AND
   (${CMAKE_C_COMPILER_ID} MATCHES "^(Clang)|(GNU)$"))
  add_compile_options(-msse2)
endif()

# Support library
add_library(safeside cache_sidechannel.cc instr.cc utils.cc)

# Defines an executable target named `demo_name` built from `demo_name.cc` and
# linked against the Safeside support library. The caller can also use the
# SYSTEMS and PROCESSORS keywords to restrict when the target should be
# created.
function(add_demo demo_name)
  cmake_parse_arguments(
    ARG  # parsed argument prefix
    ""  # boolean options
    ""  # one-value arguments
    "SYSTEMS;PROCESSORS"  # multi-value arguments
    ${ARGN}  # arguments to parse -- ARGN excludes already-named arguments
  )

  if (DEFINED ARG_SYSTEMS)
    if (NOT ${CMAKE_SYSTEM_NAME} IN_LIST ARG_SYSTEMS)
      return()
    endif()
  endif()

  if (DEFINED ARG_PROCESSORS)
    if (NOT ${CMAKE_SYSTEM_PROCESSOR} IN_LIST ARG_PROCESSORS)
      return()
    endif()
  endif()

  add_executable(${demo_name} ${demo_name}.cc)
  target_link_libraries(${demo_name} safeside)
endfunction()

# Spectre V1 PHT SA -- mistraining PHT in the same address space
add_demo(spectre_v1_pht_sa)

# Spectre V1 BTB SA -- mistraining BTB in the same address space
add_demo(spectre_v1_btb_sa)

# Spectre V4 -- speculative store bypass
add_demo(spectre_v4)

# Ret2Spec -- rewriting the RSB using recursion in the same address space
add_demo(ret2spec_sa)

# Spectre V1 BTB CA - mistraining BTB from another address space
add_demo(spectre_v1_btb_ca SYSTEMS Linux)

# Ret2Spec -- speculative execution using return stack buffers creating a
# call-ret disparity by inline assembly
add_demo(ret2spec_callret_disparity
         SYSTEMS Linux Darwin
         PROCESSORS i386 x86_64 aarch64)
if (TARGET ret2spec_callret_disparity)
  target_compile_options(ret2spec_callret_disparity
                         PRIVATE -fomit-frame-pointer)
endif()

# Spectre V3 / Meltdown
add_demo(meltdown SYSTEMS Linux PROCESSORS i686 x86_64 ppc64le)

# Spectre V2 -- branch target injection
add_demo(spectre_v2)
if(TARGET spectre_v2 AND (${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$"))
  target_compile_options(spectre_v2 PRIVATE -fomit-frame-pointer)
elseif(TARGET spectre_v2 AND (${CMAKE_SYSTEM_NAME} MATCHES "^Windows$"))
  target_compile_options(spectre_v2 PRIVATE /Ob2)
  target_compile_options(spectre_v2 PRIVATE /GL)
endif()

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
         PROCESSORS i686 x86_64)

# Speculation over hardware breakpoint fault (execution watcher)
add_demo(speculation_over_exec_hw_breakpoint
         SYSTEMS Linux
         PROCESSORS i686 x86_64)

# Meltdown AC -- speculative fetching of unaligned data
add_demo(meltdown_ac SYSTEMS Linux PROCESSORS i686 x86_64)

# Meltdown DE -- speculative computation with division by zero remainder
add_demo(meltdown_de SYSTEMS Linux PROCESSORS i686 x86_64)
