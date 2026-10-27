cmake_minimum_required(VERSION 3.25)
project(agino-gpu VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core DBus)

option(BUILD_TESTING "Build the unit tests" ON)
option(AGINO_GPU_BUILD_PLUGINS "Build the client library and the DDE plugins" ON)
if(AGINO_GPU_BUILD_PLUGINS)
    find_package(Dtk6 REQUIRED COMPONENTS Core)
endif()

add_compile_options(-Wall -Wextra)
add_compile_definitions(QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII QT_NO_KEYWORDS)

set(AGINO_GPU_LIBEXECDIR "${CMAKE_INSTALL_FULL_LIBEXECDIR}/agino-gpu")

add_subdirectory(src/lib)
add_subdirectory(src/daemon)
add_subdirectory(src/run)
if(AGINO_GPU_BUILD_PLUGINS)
    add_subdirectory(src/client)
    add_subdirectory(src/quickpanel)
    add_subdirectory(src/dcc)
endif()
add_subdirectory(data)

if(BUILD_TESTING)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
