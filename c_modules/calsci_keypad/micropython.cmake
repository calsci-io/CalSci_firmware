##############################################################################
# calsci_keypad build rules
##############################################################################
add_library(usermod_calsci_keypad INTERFACE)

target_sources(usermod_calsci_keypad INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/modcalsci_keypad.c
)

target_include_directories(usermod_calsci_keypad INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(usermod INTERFACE usermod_calsci_keypad)
