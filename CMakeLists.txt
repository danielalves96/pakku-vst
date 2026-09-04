cmake_minimum_required(VERSION 3.22)
project(Pakku VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
# Turned off in CI, where the plugin is packaged rather than installed and the
# system destination may not be writable.
option(PAKKU_COPY_AFTER_BUILD "Copy the plugin into the system folders after building" ON)

add_subdirectory(JUCE)
add_subdirectory(src)
add_subdirectory(tools/probe)
add_subdirectory(tools/guishot)
add_subdirectory(tools/ostest)
add_subdirectory(tools/dsptest)
add_subdirectory(tools/figures)
