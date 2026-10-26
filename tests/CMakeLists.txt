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
