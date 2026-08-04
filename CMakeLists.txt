cmake_minimum_required(VERSION 3.20)

project(vix_engine VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(VIX_ENGINE_BUILD_TESTS
  "Build Vix Engine tests"
  OFF
)

option(VIX_ENGINE_FETCH_JSON
  "Auto-fetch nlohmann_json if vix::json is unavailable"
  ON
)

set(VIX_ENGINE_UMBRELLA_BUILD OFF)

if (DEFINED VIX_UMBRELLA_BUILD AND VIX_UMBRELLA_BUILD)
  set(VIX_ENGINE_UMBRELLA_BUILD ON)
  set(VIX_ENGINE_EXPORT_SET VixTargets)
else()
  set(VIX_ENGINE_EXPORT_SET vix_engineTargets)
endif()

# ---------------------------------------------------------------------------
# JSON dependency resolution
# ---------------------------------------------------------------------------

set(VIX_ENGINE_JSON_TARGET "")
set(VIX_ENGINE_JSON_PROVIDER "")
set(VIX_ENGINE_FIND_JSON_DEPENDENCY "")
set(VIX_ENGINE_EXPORT_FETCHED_JSON OFF)

if (VIX_ENGINE_UMBRELLA_BUILD)
  if (NOT TARGET vix::json)
    message(
      FATAL_ERROR
      "[engine] Umbrella build requires vix::json to exist before engine."
    )
  endif()

  set(VIX_ENGINE_JSON_TARGET vix::json)
  set(VIX_ENGINE_JSON_PROVIDER umbrella)
else()
  # Prefer the sibling Vix JSON module.
  if (NOT TARGET vix::json)
    if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/../json/CMakeLists.txt")
      set(VIX_JSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
      set(VIX_JSON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
      set(VIX_JSON_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)

      add_subdirectory(
        "${CMAKE_CURRENT_LIST_DIR}/../json"
        "${CMAKE_CURRENT_BINARY_DIR}/vix-json"
      )
    else()
      find_package(vix_json CONFIG QUIET)
    endif()
  endif()

  if (TARGET vix::json)
    set(VIX_ENGINE_JSON_TARGET vix::json)
    set(VIX_ENGINE_JSON_PROVIDER vix_json)
    set(
      VIX_ENGINE_FIND_JSON_DEPENDENCY
      "find_dependency(vix_json CONFIG)"
    )
  else()
    # Try an installed nlohmann_json package.
    find_package(nlohmann_json CONFIG QUIET)

    if (TARGET nlohmann_json::nlohmann_json)
      set(
        VIX_ENGINE_JSON_TARGET
        nlohmann_json::nlohmann_json
      )

      set(VIX_ENGINE_JSON_PROVIDER nlohmann_json)
      set(
        VIX_ENGINE_FIND_JSON_DEPENDENCY
        "find_dependency(nlohmann_json CONFIG)"
      )
    elseif (VIX_ENGINE_FETCH_JSON)
      # Fetch only the source tree. Do not call
      # FetchContent_MakeAvailable(), because that creates a regular
      # nlohmann_json target that cannot be referenced by our export set.
      include(FetchContent)

      FetchContent_Declare(
        nlohmann_json_fallback
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
      )

      FetchContent_GetProperties(nlohmann_json_fallback)

      if (NOT nlohmann_json_fallback_POPULATED)
        FetchContent_Populate(nlohmann_json_fallback)
      endif()

      if (
        NOT EXISTS
        "${nlohmann_json_fallback_SOURCE_DIR}/include/nlohmann/json.hpp"
      )
        message(
          FATAL_ERROR
          "[engine] Fetched nlohmann_json headers are unavailable."
        )
      endif()

      # This target belongs to the engine package and can therefore be
      # exported together with vix_engine.
      add_library(vix_engine_json INTERFACE)

      target_include_directories(
        vix_engine_json
        INTERFACE
          $<BUILD_INTERFACE:${nlohmann_json_fallback_SOURCE_DIR}/include>
          $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
      )

      set_target_properties(
        vix_engine_json
        PROPERTIES
          EXPORT_NAME engine_json
      )

      add_library(vix::engine_json ALIAS vix_engine_json)

      set(VIX_ENGINE_JSON_TARGET vix_engine_json)
      set(VIX_ENGINE_JSON_PROVIDER fetched)
      set(VIX_ENGINE_EXPORT_FETCHED_JSON ON)
      set(VIX_ENGINE_FIND_JSON_DEPENDENCY "")
    else()
      message(
        FATAL_ERROR
        "[engine] vix::json or nlohmann_json is required by vix::engine."
      )
    endif()
  endif()
endif()

if (NOT VIX_ENGINE_JSON_TARGET)
  message(
    FATAL_ERROR
    "[engine] Internal error: no JSON target was selected."
  )
endif()

# ---------------------------------------------------------------------------
# Engine library
# ---------------------------------------------------------------------------

file(
  GLOB_RECURSE
  VIX_ENGINE_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

if (NOT VIX_ENGINE_SOURCES)
  message(
    FATAL_ERROR
    "[engine] No source files were found under src/."
  )
endif()

add_library(vix_engine STATIC ${VIX_ENGINE_SOURCES})
add_library(vix::engine ALIAS vix_engine)

target_compile_features(
  vix_engine
  PUBLIC
    cxx_std_20
)

target_include_directories(
  vix_engine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if (VIX_ENGINE_UMBRELLA_BUILD)
  target_link_libraries(
    vix_engine
    PUBLIC
      "${VIX_ENGINE_JSON_TARGET}"
  )
elseif (VIX_ENGINE_JSON_PROVIDER STREQUAL "vix_json")
  target_link_libraries(
    vix_engine
    PUBLIC
      "$<BUILD_INTERFACE:${VIX_ENGINE_JSON_TARGET}>"
      "$<INSTALL_INTERFACE:vix::json>"
  )
elseif (VIX_ENGINE_JSON_PROVIDER STREQUAL "nlohmann_json")
  target_link_libraries(
    vix_engine
    PUBLIC
      "$<BUILD_INTERFACE:${VIX_ENGINE_JSON_TARGET}>"
      "$<INSTALL_INTERFACE:nlohmann_json::nlohmann_json>"
  )
elseif (VIX_ENGINE_JSON_PROVIDER STREQUAL "fetched")
  target_link_libraries(
    vix_engine
    PUBLIC
      vix_engine_json
  )
else()
  message(
    FATAL_ERROR
    "[engine] Unsupported JSON provider: ${VIX_ENGINE_JSON_PROVIDER}"
  )
endif()

set_target_properties(
  vix_engine
  PROPERTIES
    OUTPUT_NAME vix_engine
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME engine
)

# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------

if (VIX_ENGINE_EXPORT_FETCHED_JSON)
  install(
    TARGETS
      vix_engine
      vix_engine_json
    EXPORT ${VIX_ENGINE_EXPORT_SET}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
else()
  install(
    TARGETS vix_engine
    EXPORT ${VIX_ENGINE_EXPORT_SET}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
endif()

if (NOT VIX_ENGINE_UMBRELLA_BUILD)
  install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
      PATTERN "*.hpp"
      PATTERN "*.h"
  )

  # When nlohmann_json was fetched, its headers become part of the
  # standalone engine package.
  if (VIX_ENGINE_EXPORT_FETCHED_JSON)
    install(
      DIRECTORY
        "${nlohmann_json_fallback_SOURCE_DIR}/include/nlohmann"
      DESTINATION
        "${CMAKE_INSTALL_INCLUDEDIR}"
      FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "*.h"
    )
  endif()

  configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/vix_engineConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/vix_engineConfig.cmake"
    INSTALL_DESTINATION
      "${CMAKE_INSTALL_LIBDIR}/cmake/vix_engine"
  )

  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/vix_engineConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  install(
    FILES
      "${CMAKE_CURRENT_BINARY_DIR}/vix_engineConfig.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/vix_engineConfigVersion.cmake"
    DESTINATION
      "${CMAKE_INSTALL_LIBDIR}/cmake/vix_engine"
  )

  install(
    EXPORT vix_engineTargets
    FILE vix_engineTargets.cmake
    NAMESPACE vix::
    DESTINATION
      "${CMAKE_INSTALL_LIBDIR}/cmake/vix_engine"
  )
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

if (VIX_ENGINE_BUILD_TESTS)
  include(CTest)
  enable_testing()

  if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
  else()
    message(
      FATAL_ERROR
      "[engine] VIX_ENGINE_BUILD_TESTS=ON, but tests/CMakeLists.txt "
      "does not exist."
    )
  endif()
endif()

message(STATUS "vix::engine configured (${PROJECT_VERSION})")
message(STATUS "[engine] JSON target: ${VIX_ENGINE_JSON_TARGET}")
message(STATUS "[engine] JSON provider: ${VIX_ENGINE_JSON_PROVIDER}")
