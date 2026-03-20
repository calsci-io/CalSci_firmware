##############################################################################
# calsci_help user-module build rules
##############################################################################
add_library(usermod_calsci_help INTERFACE)

target_sources(usermod_calsci_help INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/modcalsci_help.c
)

target_include_directories(usermod_calsci_help INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(usermod INTERFACE usermod_calsci_help)
