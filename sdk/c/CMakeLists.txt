cmake_minimum_required(VERSION 3.20)
project(quantarng
    VERSION   1.1.0
    LANGUAGES C
    DESCRIPTION "QuantaRNG USB SDK")

# ABI revision. Raised to 2 in 1.1.0: qrng_health_status_t gained fields,
# so a 1.0.x caller must be recompiled against the current header.
set(QUANTARNG_SOVERSION 2)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(BUILD_EXAMPLES   "Build example programs"             ON)
option(BUILD_TESTS      "Build unit tests"                   OFF)
option(BUILD_SHARED     "Build shared library (.so/.dylib/.dll)" ON)

# ---- Library sources ----
set(SRC
    src/quantarng.c
    src/qrng_reply.c
)

if(WIN32)
    list(APPEND SRC src/platform_win32.c)
else()
    list(APPEND SRC src/platform_posix.c)
endif()

# ---- Library target ----
if(BUILD_SHARED)
    add_library(quantarng SHARED ${SRC})
else()
    add_library(quantarng STATIC ${SRC})
endif()

target_include_directories(quantarng
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(quantarng PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION ${QUANTARNG_SOVERSION}
    PUBLIC_HEADER include/quantarng.h
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

# Put runtime/archive outputs in a single directory so examples link cleanly
set_target_properties(quantarng PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY         "${CMAKE_BINARY_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY         "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
)

if(WIN32)
    target_link_libraries(quantarng PRIVATE setupapi)
    target_compile_definitions(quantarng PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(MSVC)
    target_compile_options(quantarng PRIVATE /W4)
else()
    target_compile_options(quantarng PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes)
endif()

# ---- Examples ----
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ---- Tests ----
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---- Install ----
include(GNUInstallDirs)

install(TARGETS quantarng
    EXPORT  quantarngTargets
    LIBRARY       DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE       DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME       DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT quantarngTargets
    FILE quantarngTargets.cmake
    NAMESPACE quantarng::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/quantarng
)

# ---- pkg-config ----
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/quantarng.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/quantarng.pc
    @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/quantarng.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
