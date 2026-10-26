# DDE dock quick-panel plugin (dde-tray-loader 2.0.38, PluginsItemInterfaceV2).
# Out of tree: only the headers of dde-tray-loader-dev are used.

# DTK headers construct QStrings from char literals.
get_directory_property(_defs COMPILE_DEFINITIONS)
list(REMOVE_ITEM _defs QT_NO_CAST_FROM_ASCII)
set_directory_properties(PROPERTIES COMPILE_DEFINITIONS "${_defs}")

find_package(DdeTrayLoader REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets DBus LinguistTools)
find_package(Dtk6 REQUIRED COMPONENTS Widget)

add_library(agino-gpu-quickpanel MODULE
    plugin.cpp
    plugin.h
    tile.cpp
    tile.h
    panel.cpp
    panel.h
    modeswitcher.cpp
    modeswitcher.h
    bannerwidget.cpp
    bannerwidget.h
    modelist.cpp
    modelist.h
    failurenotifier.cpp
    failurenotifier.h
    controlcenter.cpp
    controlcenter.h
    qptexts.h
    agino-gpu.json
)
target_compile_definitions(agino-gpu-quickpanel PRIVATE QT_PLUGIN)
target_include_directories(agino-gpu-quickpanel PRIVATE ${DDE_TRAY_LOADER_INCLUDE_DIR})
target_link_libraries(agino-gpu-quickpanel PRIVATE
    agino-gpu-client
    Dtk6::Widget
    Qt6::Widgets
    Qt6::DBus
)
install(TARGETS agino-gpu-quickpanel LIBRARY DESTINATION lib/dde-dock/plugins)

# Plugin-local strings (qptexts.h), context "AginoGpuQuickPanel"; shared ones live in the client.
set(AGINO_GPU_QP_TRANSLATIONS_DIR "${CMAKE_INSTALL_FULL_DATADIR}/agino-gpu/translations")
target_compile_definitions(agino-gpu-quickpanel PRIVATE
    AGINO_GPU_QP_TRANSLATIONS_DIR="${AGINO_GPU_QP_TRANSLATIONS_DIR}")
file(GLOB AGINO_GPU_QP_TS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/translations/agino-gpu-quickpanel_*.ts)
qt_add_lrelease(agino-gpu-quickpanel TS_FILES ${AGINO_GPU_QP_TS} QM_FILES_OUTPUT_VARIABLE AGINO_GPU_QP_QM)
install(FILES ${AGINO_GPU_QP_QM} DESTINATION "${AGINO_GPU_QP_TRANSLATIONS_DIR}")
