cmake_minimum_required(VERSION 3.15)
project(PoliteShutdown)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set the compiler flags
if (MSVC)
    add_compile_options(/EHa /W4) # Compiler-wide options
endif()

# Define the source files
set(SOURCES
    src/EasyWin.cpp
    src/EasyWinPrivilege.cpp
    src/EasyWinReg.cpp
    src/EasyWinSID.cpp
    src/EasyWinShutdown.cpp
    src/EasyWinSync.cpp
    src/EasyWinTasks.cpp
    src/LoggedOnUsers.cpp
    src/Logger.cpp
    src/PoliteShutdown.cpp
    src/Processes.cpp
    src/SettingsSource.cpp
    src/auto_handle.h
    src/main.cpp
    src/version.h
    src/version.rc
    src/winUpdate.cpp
    src/winUpdate.h
)

# Add executable for the main target
add_executable(PoliteShutdown ${SOURCES})

# Add libraries
target_link_libraries(PoliteShutdown
    shlwapi.lib
    kernel32.lib
    user32.lib
    advapi32.lib
    shell32.lib
    ole32.lib
    Secur32.lib
    Psapi.lib
    Wtsapi32.lib
)

# Set compile options and output directories for different build types
set_target_properties(PoliteShutdown PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release
)

# Debug build settings
target_compile_definitions(PoliteShutdown PRIVATE $<$<CONFIG:Debug>:DEBUG _DEBUG>)
target_compile_options(PoliteShutdown PRIVATE $<$<CONFIG:Debug>:/MTd>)

# Release build settings
target_compile_options(PoliteShutdown PRIVATE $<$<CONFIG:Release>:/MT /O1>)

