##############################################################################
# calsci_runtime build rules
##############################################################################
add_library(usermod_calsci_runtime INTERFACE)

target_sources(usermod_calsci_runtime INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/calsci_runtime.c
)

target_include_directories(usermod_calsci_runtime INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(usermod INTERFACE usermod_calsci_runtime)
