add_executable(unmitigated microbenchmark.cc)
# Compile with -O0 so the array manipulation doesn't get optimized away by the
# compiler.
# FIXME: Replace this with tricks like this:
# https://github.com/travisdowns/zero-fill-bench/blob/c30d8fc88d3f560ba165ef756167a2a703678134/opt-control.hpp#L11-L36
target_compile_options(unmitigated PUBLIC -O0)

# Speculative load hardening is only available in Clang >= 8.0.0.
# Avoid VERSION_GREATER_EQUAL so we work with CMake < 3.7.
if (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") AND
    (NOT "${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS "8.0.0"))
  add_executable(slh_mitigated microbenchmark.cc)
  # Compile with -O0 so the array manipulation doesn't get optimized away by the
  # compiler.
  target_compile_options(slh_mitigated PUBLIC -mspeculative-load-hardening -O0)
endif()
