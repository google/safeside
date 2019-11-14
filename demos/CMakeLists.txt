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

# Takes care of the common case for demo programs: an executable target
# that compiles a file of the same name and links against the Safeside
# support library.
function(add_demo demo_name)
  add_executable(${demo_name} ${demo_name}.cc)
  target_link_libraries(${demo_name} safeside)
endfunction(add_demo)

# Spectre V1 PHT SA -- mistraining PHT in the same address space
add_demo(spectre_v1_pht_sa)

# Spectre V1 BTB SA -- mistraining BTB in the same address space
add_demo(spectre_v1_btb_sa)

# Spectre V4 -- speculative store bypass
add_demo(spectre_v4)

# Ret2Spec -- rewriting the RSB using recursion in the same address space
add_demo(ret2spec_sa)

if(${CMAKE_SYSTEM_NAME} MATCHES "^(Linux)$")
  # Spectre V1 BTB CA - mistraining BTB from another address space
  add_demo(spectre_v1_btb_ca)
endif()

if((${CMAKE_SYSTEM_NAME} MATCHES "^(Linux)|(Darwin)$") AND
   (${CMAKE_SYSTEM_PROCESSOR} MATCHES "^(i.86)|(x86_64)|(aarch64)$"))
  # Ret2Spec -- speculative execution using return stack buffers creating a
  # call-ret disparity by inline assembly
  add_demo(ret2spec_callret_disparity)
  target_compile_options(ret2spec_callret_disparity PRIVATE -fomit-frame-pointer)
endif()

if((${CMAKE_SYSTEM_NAME} MATCHES "^(Linux)$") AND
   (${CMAKE_SYSTEM_PROCESSOR} MATCHES "^(i.86)|(x86_64)|(ppc64le)$"))
  # Spectre V3 / Meltdown
  add_demo(meltdown)

  # L1 terminal fault -- Foreshadow OS -- Meltdown P
  add_demo(l1tf)
endif()

if((${CMAKE_SYSTEM_NAME} MATCHES "^(Linux)$") AND
   (${CMAKE_SYSTEM_PROCESSOR} MATCHES "^(aarch64)$"))
  # Speculation over ERET, HVC and SMC instructions
  add_demo(eret_hvc_smc_wrapper)

  # Speculation over syscall
  add_demo(speculation_over_syscall)

  # Meltdown UD -- speculation over an undefined instruction
  add_demo(meltdown_ud)
endif()

if((${CMAKE_SYSTEM_NAME} MATCHES "^(Linux)|(Darwin)$") AND
   (${CMAKE_SYSTEM_PROCESSOR} MATCHES "^(i.86)$"))
   # Meltdown BR - speculation over the ia32 bounds check instruction
  add_demo(meltdown_br)
endif()

if((${CMAKE_SYSTEM_NAME} MATCHES "^(Linux)$") AND
   (${CMAKE_SYSTEM_PROCESSOR} MATCHES "^(i.86)$"))
  # Meltdown SS -- speculative reading from non present segments and outside of
  # segment limits
  add_demo(meltdown_ss)
endif()

if((${CMAKE_SYSTEM_NAME} MATCHES "^(Linux)|(Darwin)$") AND
   (${CMAKE_SYSTEM_PROCESSOR} MATCHES "^(i.86)$"))
  # Meltdown OF -- speculative fetching from an overflowing address after an
  # INTO check
  add_demo(meltdown_of)
endif()

if((${CMAKE_SYSTEM_NAME} MATCHES "^(Linux)$") AND
   (${CMAKE_SYSTEM_PROCESSOR} MATCHES "^(i.86)|(x86_64)$"))
  # Speculation over hardware breakpoint trap (read watcher)
  add_demo(speculation_over_read_hw_breakpoint)

  # Speculation over hardware breakpoint fault (execution watcher)
  add_demo(speculation_over_exec_hw_breakpoint)

  # Meltdown AC -- speculative fetching of unaligned data
  add_demo(meltdown_ac)

  # Meltdown DE -- speculative computation with division by zero remainder
  add_demo(meltdown_de)
endif()
