ENABLE_LANGUAGE(ASM)

file(GLOB LIB_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
file(GLOB LIB_CSRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "aarch64")
file(GLOB LIB_SSRC ${CMAKE_CURRENT_SOURCE_DIR}/arm64/*.S)
message(STATUS "ARCH: ARM64")
else("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "armv7-a")
file(GLOB LIB_SSRC ${CMAKE_CURRENT_SOURCE_DIR}/arm32/*.S)
message(STATUS "ARCH: ARM32")
endif()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g -Wall -O3 -frtti -Wno-uninitialized")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -Wall -O3 -frtti -Wno-uninitialized")

add_library(arm_backend_sgemm_obj OBJECT ${LIB_CSRC} ${LIB_SSRC})

set(ARM_INSTALL_DIR "${PROJECT_BINARY_DIR}/install/feather_backend_sgemm/")

