find_package(PkgConfig QUIET)
set(AGINO_GPU_SYSTEMD_UNITDIR "")
if(PkgConfig_FOUND)
    pkg_get_variable(AGINO_GPU_SYSTEMD_UNITDIR systemd systemdsystemunitdir)
endif()
if(NOT AGINO_GPU_SYSTEMD_UNITDIR)
    set(AGINO_GPU_SYSTEMD_UNITDIR "${CMAKE_INSTALL_PREFIX}/lib/systemd/system")
endif()

set(GEN "${CMAKE_CURRENT_BINARY_DIR}")

# Files that carry the libexec path.
configure_file(org.agino.Gpu1.service.in "${GEN}/org.agino.Gpu1.service" @ONLY)
configure_file(agino-gpud.service.in "${GEN}/agino-gpud.service" @ONLY)
configure_file(agino-gpu-apply.service.in "${GEN}/agino-gpu-apply.service" @ONLY)
configure_file(82-agino-gpu-integrated.rules.in "${GEN}/82-agino-gpu-integrated.rules" @ONLY)
configure_file(90-agino-gpu.conf.in "${GEN}/90-agino-gpu.conf" @ONLY)

# D-Bus
install(FILES org.agino.Gpu1.xml DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/interfaces")
install(FILES "${GEN}/org.agino.Gpu1.service" DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/system-services")
install(FILES org.agino.Gpu1.conf DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/system.d")

# polkit
install(FILES org.agino.gpu.policy DESTINATION "${CMAKE_INSTALL_DATADIR}/polkit-1/actions")

# systemd
install(FILES "${GEN}/agino-gpud.service" "${GEN}/agino-gpu-apply.service"
        DESTINATION "${AGINO_GPU_SYSTEMD_UNITDIR}")

# udev (the NVIDIA runtime PM rule lives in agino-kernel-config, not here)
install(FILES "${GEN}/82-agino-gpu-integrated.rules" DESTINATION lib/udev/rules.d)

# lightdm
install(FILES "${GEN}/90-agino-gpu.conf" DESTINATION "${CMAKE_INSTALL_DATADIR}/lightdm/lightdm.conf.d")
install(PROGRAMS display-setup DESTINATION "${AGINO_GPU_LIBEXECDIR}")

# Directories the hardened agino-gpud.service lists in ReadWritePaths= (they must exist).
install(DIRECTORY DESTINATION "${CMAKE_INSTALL_FULL_SYSCONFDIR}/agino-gpu")
install(DIRECTORY DESTINATION "${CMAKE_INSTALL_FULL_SYSCONFDIR}/X11/xorg.conf.d")

# User DConfig of the rules (read by the plugins and the DAM patch)
if(AGINO_GPU_BUILD_PLUGINS)
    install(FILES dconfig/org.agino.gpu.json DESTINATION "${CMAKE_INSTALL_DATADIR}/dsg/configs/agino-gpu")
    # The quick panel shows only the plugins named in its read-only quickPlugins list (dde-shell
    # QuickPanelProxyModel::filterAcceptsRow, despite what the key's description says). This
    # replaces the whole list: the dde-shell 2.0.52 default plus agino-gpu after the connection
    # tiles. Revisit when dde-shell changes its default.
    install(FILES dconfig/overrides/org.deepin.ds.dock.tray.json
            DESTINATION "${CMAKE_INSTALL_DATADIR}/dsg/configs/overrides/org.deepin.dde.shell/org.deepin.ds.dock.tray"
            RENAME 50-agino-gpu.json)
endif()
