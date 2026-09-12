cmake_minimum_required(VERSION 3.16)

# ---------------------------------------------------------------------------
# cloxBooster - a Lox dialect interpreter (VM)
# ---------------------------------------------------------------------------
project(cloxBooster
  VERSION 0.1.0
  DESCRIPTION "cloxBooster - a fast Lox dialect interpreter"
  LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options -------------------------------------------------------------------
option(CLOXBOOSTER_BUILD_SHARED "Build shared library" OFF)

# Compiler flag selection ---------------------------------------------------
# Mimics the flags used in cloxBooster.vcxproj so CMake builds behave like the
# Visual Studio workflow where possible.
if(MSVC)
  # Exceptions are disabled project-wide in the original MSBuild config.
  # C and C++ are handled separately so no conflicting /EH flags remain.
  add_compile_options($<$<COMPILE_LANGUAGE:C>:/EHs-c->)
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/EH->)
  # AdvancedVectorExtensions2 == SSE4.2
  add_compile_options(/arch:SSE4.2)
else()
  add_compile_options(-fno-exceptions)
endif()

# Warnings (keep parity with WarningLevel Level3)
if(MSVC)
  add_compile_options(/W3)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra)
endif()

# Debug / release build type ------------------------------------------------
# For single-config generators (Makefiles/Ninja) default to Release unless a
# build type is already set (e.g. by CMakePresets). Multi-config generators
# (Visual Studio, Ninja Multi-Config) manage this via CMAKE_CONFIGURATION_TYPES.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# No third-party dependencies: hashing uses the built-in MurmurHash64 and
# allocation goes through the standard library.

# Core library --------------------------------------------------------------
set(CLOXBOOSTER_CORE_SOURCES
  src/file.c
  src/hash.c
  src/chunk.c
  src/compiler.c
  src/debug.c
  src/entrance.c
  src/gc.c
  src/lineArray.c
  src/memory.c
  src/numberTable.c
  src/object.c
  src/scanner.c
  src/stringTable.c
  src/table.c
  src/value.c
  src/vm.c
)

add_library(cloxBoosterCore STATIC ${CLOXBOOSTER_CORE_SOURCES})
target_include_directories(cloxBoosterCore PUBLIC
  ${CMAKE_SOURCE_DIR}/src
)

# Executable ----------------------------------------------------------------
add_executable(cloxBooster main.c)
target_link_libraries(cloxBooster PRIVATE cloxBoosterCore)
