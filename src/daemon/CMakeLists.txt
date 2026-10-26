# Daemon and boot helpers. Installed to ${AGINO_GPU_LIBEXECDIR} (set by the top-level file).

set(AGINO_GPU_DAEMON_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/common")

add_executable(agino-gpud
    gpud/main.cpp
    gpud/polkit.h
    gpud/polkit.cpp
    gpud/gpuservice.h
    gpud/gpuservice.cpp
)
target_include_directories(agino-gpud PRIVATE "${AGINO_GPU_DAEMON_COMMON_DIR}" gpud)
target_link_libraries(agino-gpud PRIVATE agino-gpu agino-gpu-dbus Qt6::Core Qt6::DBus)

add_executable(agino-gpu-apply apply/main.cpp)
target_include_directories(agino-gpu-apply PRIVATE "${AGINO_GPU_DAEMON_COMMON_DIR}")
target_link_libraries(agino-gpu-apply PRIVATE agino-gpu Qt6::Core)

add_executable(agino-gpu-integrated-remove integrated-remove/main.cpp)
target_include_directories(agino-gpu-integrated-remove PRIVATE "${AGINO_GPU_DAEMON_COMMON_DIR}")
target_link_libraries(agino-gpu-integrated-remove PRIVATE agino-gpu Qt6::Core)

install(TARGETS agino-gpud agino-gpu-apply agino-gpu-integrated-remove
        RUNTIME DESTINATION "${AGINO_GPU_LIBEXECDIR}")
