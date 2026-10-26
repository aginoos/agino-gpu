# libagino-gpu: static, position independent, linked into the daemon, the tools and (P2) the
# quick-panel and control-center plugins. Not installed: there is no public ABI.
add_library(agino-gpu STATIC
    root.cpp
    detect.cpp
    mode.cpp
    dgpuusers.cpp
    offloadenv.cpp
    configgen.cpp
    rules.cpp
)
set_target_properties(agino-gpu PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(agino-gpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(agino-gpu PUBLIC Qt6::Core)

# The org.agino.Gpu1 wire types, shared by the daemon and the client library.
add_library(agino-gpu-dbus STATIC dbustypes.cpp)
set_target_properties(agino-gpu-dbus PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(agino-gpu-dbus PUBLIC agino-gpu Qt6::DBus)
