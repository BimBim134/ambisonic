cmake_minimum_required(VERSION 3.22)

project(ambisonic LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Ambisonic order is fixed at configure time. Output channel count is
# (order + 1)^2. Reconfigure (e.g. -DAMBI_ORDER=1) to build a different order.
set(AMBI_ORDER 3 CACHE STRING "Ambisonic order for the VST3 plugin (0-7)")
if(NOT AMBI_ORDER MATCHES "^[0-7]$")
  message(FATAL_ERROR "AMBI_ORDER must be an integer between 0 and 7 (got '${AMBI_ORDER}')")
endif()

find_package(Threads REQUIRED)

# =====================================================================
# Shared DSP core (used by both the CLI and the plugin)
# =====================================================================
add_library(sde_dsp STATIC
  src/encoder.cpp
  src/spherical_harmonics.cpp
)
target_include_directories(sde_dsp PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MSVC)
  target_compile_options(sde_dsp PRIVATE /W4 /permissive-)
else()
  target_compile_options(sde_dsp PRIVATE -Wall -Wextra)
endif()

# =====================================================================
# Command-line tool
# =====================================================================
add_library(sde_core STATIC
  src/audio.cpp
)
target_include_directories(sde_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_link_libraries(sde_core PUBLIC Threads::Threads sde_dsp)

add_executable(ambisonic src/main.cpp)
target_link_libraries(ambisonic PRIVATE sde_core)
if(MSVC)
  target_compile_options(sde_core PRIVATE /W4 /permissive-)
  target_compile_options(ambisonic PRIVATE /W4 /permissive-)
else()
  target_compile_options(sde_core PRIVATE -Wall -Wextra)
  target_compile_options(ambisonic PRIVATE -Wall -Wextra)
endif()

# =====================================================================
# VST3 plugin (JUCE)
# =====================================================================
include(FetchContent)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 9.0.4
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# Embedded UI fonts (JetBrains Mono, SIL-OFL; see assets/fonts/OFL.txt).
# Symbols are generated as BinaryData::JetBrainsMonoRegular_ttf / ...Bold_ttf.
juce_add_binary_data(ambi_fonts
  SOURCES
    assets/fonts/JetBrainsMonoRegular.ttf
    assets/fonts/JetBrainsMonoBold.ttf
)

# Install to the per-user VST3 folder to avoid needing administrator rights on
# Windows (Program Files/Common Files/VST3 usually requires elevation).
set(VST3_INSTALL_DIR "$ENV{LOCALAPPDATA}/Common/VST3")

juce_add_plugin(ambi_encoder
  COMPANY_NAME "ambisonic"
  PLUGIN_MANUFACTURER_CODE Ambi
  PLUGIN_CODE AmbE
  PRODUCT_NAME "The Potate"
  DESCRIPTION "Stereo to SN3D/ACN ambisonic encoder"
  VERSION "0.1.0"
  FORMATS VST3
  VST3_CATEGORIES Fx SoundField
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  VST3_COPY_DIR "${VST3_INSTALL_DIR}"
)

target_sources(ambi_encoder PRIVATE
  src/PluginProcessor.cpp
  src/PluginEditor.cpp
  src/LookAndFeel.cpp
)

target_include_directories(ambi_encoder PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(ambi_encoder PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  AMBI_ORDER=${AMBI_ORDER}
)

target_link_libraries(ambi_encoder
  PRIVATE
    sde_dsp
    ambi_fonts
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
