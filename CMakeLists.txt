cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Network)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui Qt6::Network)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/panel.cpp
    src/panel.hpp
    src/services.cpp
    src/services.hpp
)

# Credenziali delle app OAuth (Twitch, Kick, YouTube) integrate nel plugin.
# Si passano come variabili d'ambiente MSIP_* in fase di build (in CI arrivano dai GitHub Secrets).
set(MSIP_TWITCH_CLIENT_ID "$ENV{MSIP_TWITCH_CLIENT_ID}")
set(MSIP_TWITCH_CLIENT_SECRET "$ENV{MSIP_TWITCH_CLIENT_SECRET}")
set(MSIP_KICK_CLIENT_ID "$ENV{MSIP_KICK_CLIENT_ID}")
set(MSIP_KICK_CLIENT_SECRET "$ENV{MSIP_KICK_CLIENT_SECRET}")
set(MSIP_YOUTUBE_CLIENT_ID "$ENV{MSIP_YOUTUBE_CLIENT_ID}")
set(MSIP_YOUTUBE_CLIENT_SECRET "$ENV{MSIP_YOUTUBE_CLIENT_SECRET}")
foreach(
  _var
  MSIP_TWITCH_CLIENT_ID
  MSIP_TWITCH_CLIENT_SECRET
  MSIP_KICK_CLIENT_ID
  MSIP_KICK_CLIENT_SECRET
  MSIP_YOUTUBE_CLIENT_ID
  MSIP_YOUTUBE_CLIENT_SECRET
)
  string(STRIP "${${_var}}" ${_var})
  if(NOT "${${_var}}" MATCHES "^[A-Za-z0-9._~-]*$")
    message(FATAL_ERROR "${_var} contiene caratteri non validi: ricopia il valore dal sito della piattaforma.")
  endif()
endforeach()
configure_file(src/credentials.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/generated/credentials.hpp" @ONLY)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

if(WIN32)
  # Rete HTTPS tramite WinHTTP (il Qt di OBS non include il backend TLS)
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/winhttp-client.cpp src/winhttp-client.hpp)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE winhttp)
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
