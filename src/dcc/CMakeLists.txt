# DDE control-center plugin "gpu" (dde-control-center 6.1, V25 plugin interface): the page
# display > Graphics with the GPU list, the mode cards and the always-use-the-dGPU rules.
# The plugin name must match the DccObject name in qml/Gpu.qml.

# DTK headers construct QStrings from char literals.
get_directory_property(_defs COMPILE_DEFINITIONS)
list(REMOVE_ITEM _defs QT_NO_CAST_FROM_ASCII)
set_directory_properties(PROPERTIES COMPILE_DEFINITIONS "${_defs}")

set(PLUGIN_NAME gpu)

# LinguistTools: dcc_handle_plugin_translation; Qml/Quick: the QML module of the plugin.
find_package(Qt6 REQUIRED COMPONENTS Core DBus Qml Quick LinguistTools)
find_package(Dtk6 REQUIRED COMPONENTS Core)
find_package(DdeControlCenter REQUIRED)

add_library(dcc-gpu MODULE
    plugin.cpp
    gpupage.cpp
    gpupage_switch.cpp
    gpubanner.cpp
    appcatalog.cpp
    addappmodel.cpp
    gpupage.h
    gpubanner.h
    appcatalog.h
    addappmodel.h
)
target_link_libraries(dcc-gpu PRIVATE
    Dde::Control-Center
    agino-gpu-client
    Qt6::Core
    Qt6::DBus
    Qt6::Qml
    Qt6::Quick
    Dtk6::Core
)

dcc_install_plugin(NAME ${PLUGIN_NAME} TARGET dcc-gpu)
dcc_handle_plugin_translation(NAME ${PLUGIN_NAME})
