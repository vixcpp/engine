cmake_minimum_required(VERSION 3.20)

project(vix_engine VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(VIX_ENGINE_BUILD_TESTS "Build Vix Engine tests" OFF)
option(VIX_ENGINE_FETCH_JSON "Auto-fetch nlohmann_json if vix::json is unavailable" ON)

if (DEFINED VIX_UMBRELLA_BUILD AND VIX_UMBRELLA_BUILD)
  set(VIX_ENGINE_EXPORT_SET VixTargets)
else()
  set(VIX_ENGINE_EXPORT_SET vix_engineTargets)
endif()

set(VIX_ENGINE_JSON_TARGET "")
set(VIX_ENGINE_JSON_PACKAGE "")

if (DEFINED VIX_UMBRELLA_BUILD AND VIX_UMBRELLA_BUILD)
  if (NOT TARGET vix::json)
    message(FATAL_ERROR "[engine] Umbrella build: vix::json must be provided before engine.")
  endif()

  set(VIX_ENGINE_JSON_TARGET vix::json)
else()
  if (NOT TARGET vix::json)
    if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/../json/CMakeLists.txt")
      set(VIX_JSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
      set(VIX_JSON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
      add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../json" "json")
    else()
      find_package(vix_json CONFIG QUIET)
    endif()
  endif()

  if (TARGET vix::json)
    set(VIX_ENGINE_JSON_TARGET vix::json)
    set(VIX_ENGINE_JSON_PACKAGE vix_json)
  else()
    find_package(nlohmann_json QUIET CONFIG)

    if (NOT TARGET nlohmann_json::nlohmann_json AND VIX_ENGINE_FETCH_JSON)
      include(FetchContent)

      FetchContent_Declare(
        nlohmann_json_fallback
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
      )

      FetchContent_MakeAvailable(nlohmann_json_fallback)
    endif()

    if (TARGET nlohmann_json::nlohmann_json)
      set(VIX_ENGINE_JSON_TARGET nlohmann_json::nlohmann_json)
      set(VIX_ENGINE_JSON_PACKAGE nlohmann_json)
    else()
      message(FATAL_ERROR "vix::json or nlohmann_json is required by vix::engine.")
    endif()
  endif()
endif()

file(GLOB_RECURSE VIX_ENGINE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(vix_engine STATIC ${VIX_ENGINE_SOURCES})
add_library(vix::engine ALIAS vix_engine)

target_compile_features(vix_engine PUBLIC cxx_std_20)

target_include_directories(vix_engine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(vix_engine
  PUBLIC
    ${VIX_ENGINE_JSON_TARGET}
)

set_target_properties(vix_engine PROPERTIES
  OUTPUT_NAME vix_engine
  VERSION ${PROJECT_VERSION}
  SOVERSION 0
  EXPORT_NAME engine
)

install(TARGETS vix_engine
  EXPORT ${VIX_ENGINE_EXPORT_SET}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if (NOT (DEFINED VIX_UMBRELLA_BUILD AND VIX_UMBRELLA_BUILD))
  install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
  )

  configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/vix_engineConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/vix_engineConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/vix_engine"
  )

  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/vix_engineConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/vix_engineConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/vix_engineConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/vix_engine"
  )

  install(EXPORT vix_engineTargets
    FILE vix_engineTargets.cmake
    NAMESPACE vix::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/vix_engine"
  )
endif()

if (VIX_ENGINE_BUILD_TESTS)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()

message(STATUS "vix::engine configured (${PROJECT_VERSION})")

