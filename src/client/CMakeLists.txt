# libagino-gpu-client: what both plugins share (state, D-Bus calls, rules, DAM sync, power).
# Static and position independent like libagino-gpu; links DTK Core for DConfig.

# DTK headers construct QStrings from char literals.
get_directory_property(_defs COMPILE_DEFINITIONS)
list(REMOVE_ITEM _defs QT_NO_CAST_FROM_ASCII)
set_directory_properties(PROPERTIES COMPILE_DEFINITIONS "${_defs}")

add_library(agino-gpu-client STATIC
    gpustate.cpp
    gpucontrol.cpp
    rulestore.cpp
    damsync.cpp
    powerwatcher.cpp
    agino-gpu-client/gpustate.h
    agino-gpu-client/gpucontrol.h
    agino-gpu-client/rulestore.h
    agino-gpu-client/damsync.h
    agino-gpu-client/powerwatcher.h
)
set_target_properties(agino-gpu-client PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(agino-gpu-client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(agino-gpu-client PUBLIC agino-gpu agino-gpu-dbus Qt6::Core Qt6::DBus Dtk6::Core)
