add_executable(agino-gpu-run main.cpp)
target_link_libraries(agino-gpu-run PRIVATE agino-gpu Qt6::Core)
install(TARGETS agino-gpu-run RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
