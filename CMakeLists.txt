cmake_minimum_required(VERSION 3.5.1)

project(safeside VERSION 0.1.0 LANGUAGES CXX)

add_library(safeside demos/cache_sidechannel.cc demos/instr.cc)

# Spectre V1 -- bounds check bypass
add_executable(spectre_v1 demos/spectre_v1.cc)
target_link_libraries(spectre_v1 safeside)

# Spectre V4 -- speculative store bypass
add_executable(spectre_v4 demos/spectre_v4.cc)
target_link_libraries(spectre_v4 safeside)


if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	
	add_subdirectory(third_party/kmod)

	# Spectre V3 / Meltdown
	add_executable(meltdown demos/meltdown.cc)
	target_link_libraries(meltdown safeside)

endif()