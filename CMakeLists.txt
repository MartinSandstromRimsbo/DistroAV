cmake_minimum_required(VERSION 3.28...3.30)

# Find Qt6
find_package(Qt6 COMPONENTS Widgets Core Network REQUIRED)

# Find CURL
find_package(CURL REQUIRED)

# Find NDI - temporarily make it optional for testing
find_library(NDI_LIBRARY NAMES NDIlib NDI PATHS /usr/local/lib /opt/homebrew/lib)
if(NOT NDI_LIBRARY)
    message(WARNING "NDI library not found. Plugin will be built without NDI support.")
    set(NDI_LIBRARY "")
endif()

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

# Only link NDI if found
if(NDI_LIBRARY)
    target_link_libraries(distroav PRIVATE ${NDI_LIBRARY})
endif()

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
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Debug"
    )
                    add_custom_command(TARGET distroav POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "$<TARGET_BUNDLE_DIR:distroav>"
                        "${CMAKE_BINARY_DIR}/frontend/plugins/distroav.plugin"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "/usr/local/lib/libndi.dylib"
                        "${CMAKE_BINARY_DIR}/frontend/Debug/MixStageCast.app/Contents/Frameworks/libndi.dylib"
                    COMMENT "Copying distroav.plugin bundle and NDI library to MixStage Cast"
                )
endif()
