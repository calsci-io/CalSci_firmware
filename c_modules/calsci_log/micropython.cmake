##############################################################################
# calsci_log shared logger build rules
##############################################################################
add_library(usermod_calsci_log INTERFACE)

target_sources(usermod_calsci_log INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/calsci_log.c
    ${CMAKE_CURRENT_LIST_DIR}/modcalsci_log.c
)

target_include_directories(usermod_calsci_log INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(usermod INTERFACE usermod_calsci_log)
