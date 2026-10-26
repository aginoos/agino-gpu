# Every tests/lib/tst_*.cpp is one QtTest executable linked against the library. Fixture trees
# live in tests/fixtures/<name>/ and are passed to the code through AginoGpu::Root.
file(GLOB AGINO_GPU_LIB_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/lib/tst_*.cpp)
foreach(src ${AGINO_GPU_LIB_TESTS})
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE agino-gpu Qt6::Test)
    target_compile_definitions(${name} PRIVATE
        AGINO_GPU_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
    add_test(NAME ${name} COMMAND ${name})
endforeach()

# tests/client/tst_*.cpp: the pure parts of the client library (no DConfig, no bus).
if(AGINO_GPU_BUILD_PLUGINS)
    file(GLOB AGINO_GPU_CLIENT_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/client/tst_*.cpp)
    foreach(src ${AGINO_GPU_CLIENT_TESTS})
        get_filename_component(name ${src} NAME_WE)
        add_executable(${name} ${src})
        target_link_libraries(${name} PRIVATE agino-gpu-client Qt6::Test)
        target_compile_definitions(${name} PRIVATE
            AGINO_GPU_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
        add_test(NAME ${name} COMMAND ${name})
    endforeach()
endif()

# tests/manual: tools run by hand (not by ctest), e.g. rendering the quick panel offscreen.
if(AGINO_GPU_BUILD_PLUGINS)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    find_package(DdeTrayLoader REQUIRED)
    add_executable(qp-harness manual/qp-harness.cpp)
    target_include_directories(qp-harness PRIVATE ${DDE_TRAY_LOADER_INCLUDE_DIR})
    target_link_libraries(qp-harness PRIVATE Qt6::Widgets)
    # the dock headers construct QStrings from char literals
    target_compile_options(qp-harness PRIVATE -UQT_NO_CAST_FROM_ASCII)
endif()
