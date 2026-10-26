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
    texts.cpp
    agino-gpu-client/gpustate.h
    agino-gpu-client/gpucontrol.h
    agino-gpu-client/rulestore.h
    agino-gpu-client/damsync.h
    agino-gpu-client/powerwatcher.h
    agino-gpu-client/texts.h
)
set_target_properties(agino-gpu-client PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(agino-gpu-client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(agino-gpu-client PUBLIC agino-gpu agino-gpu-dbus Qt6::Core Qt6::DBus Dtk6::Core)

# Shared UI strings (texts.h), context "AginoGpu"; both plugins load them.
set(AGINO_GPU_TRANSLATIONS_DIR "${CMAKE_INSTALL_FULL_DATADIR}/agino-gpu/translations")
target_compile_definitions(agino-gpu-client PRIVATE
    AGINO_GPU_TRANSLATIONS_DIR="${AGINO_GPU_TRANSLATIONS_DIR}")
find_package(Qt6 REQUIRED COMPONENTS LinguistTools)
file(GLOB AGINO_GPU_CLIENT_TS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/translations/agino-gpu_*.ts)
qt_add_lrelease(agino-gpu-client TS_FILES ${AGINO_GPU_CLIENT_TS} QM_FILES_OUTPUT_VARIABLE AGINO_GPU_CLIENT_QM)
install(FILES ${AGINO_GPU_CLIENT_QM} DESTINATION "${AGINO_GPU_TRANSLATIONS_DIR}")
