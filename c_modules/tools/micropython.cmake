##############################################################################
# tools user-module build rules
##############################################################################
add_library(usermod_tools INTERFACE)

target_sources(usermod_tools INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/modtools.c
)

target_include_directories(usermod_tools INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(usermod INTERFACE usermod_tools)
