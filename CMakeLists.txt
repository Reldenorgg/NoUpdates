cmake_minimum_required(VERSION 3.22)
project(no_updates LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    preloader_android
    GIT_REPOSITORY https://github.com/LiteLDev/preloader-android.git
    GIT_TAG 0.2.2)
FetchContent_MakeAvailable(preloader_android)

add_library(no_updates SHARED src/NoUpdates.cpp)
target_link_libraries(no_updates PRIVATE preloader log)
