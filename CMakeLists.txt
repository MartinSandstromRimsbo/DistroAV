cmake_minimum_required(VERSION 3.28...3.30)

# Find Qt6
find_package(Qt6 COMPONENTS Widgets Core Network REQUIRED)

# Find CURL
find_package(CURL REQUIRED)

# NDI: do not bundle in build; resolve at runtime via load_ndilib()
set(NDI_LIBRARY "")

# Create the plugin library
add_library(distroav MODULE)

# Set Qt properties
set_target_properties(distroav PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

# Configure plugin-support.c from template
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin-support.c.in
    ${CMAKE_CURRENT_BINARY_DIR}/plugin-support.c
    @ONLY
)

# Add only core NDI source files (excluding UI components that need frontend API)
target_sources(distroav PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/plugin-support.c
    src/obs-support/obs-app.hpp
    src/obs-support/remote-text.cpp
    src/obs-support/remote-text.hpp
    src/obs-support/shared-update.cpp
    src/obs-support/shared-update.hpp
    src/ndi-finder.h
    src/ndi-finder.cpp
    src/ndi-output.cpp
    src/ndi-source.cpp
    src/plugin-main.cpp
    src/plugin-main.h
    src/premultiplied-alpha-filter.cpp
)

# Link libraries
target_link_libraries(distroav PRIVATE
    OBS::libobs
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    CURL::libcurl
)

# Do not link NDI here; runtime `QLibrary` loads it from system SDK or app Frameworks

# Include directories
target_include_directories(distroav PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/ndi
    /usr/local/include
    /opt/homebrew/include
)

# Set output name
set_target_properties(distroav PROPERTIES OUTPUT_NAME "distroav")

# Set OBS target properties
set_target_properties_obs(distroav PROPERTIES FOLDER plugins PREFIX "")

# --- macOS plugin bundle settings ---
if(APPLE)
    set_target_properties(distroav PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "plugin"
        OUTPUT_NAME "distroav"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
    )
    add_custom_command(TARGET distroav POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
              "$<TARGET_BUNDLE_DIR:distroav>"
              "${CMAKE_BINARY_DIR}/frontend/$<CONFIG>/MixStageCast.app/Contents/PlugIns/distroav.plugin"
      COMMENT "Copying distroav.plugin into app bundle (no NDI bundling)"
    )
endif()
