##############################################################################
# st7565 user-module build rules
##############################################################################
add_library(usermod_st7565 INTERFACE)

target_sources(usermod_st7565 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/modst7565.c
    ${CMAKE_CURRENT_LIST_DIR}/st7565.c
)

target_include_directories(usermod_st7565 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

# Link this lib into the global “usermod” target
target_link_libraries(usermod INTERFACE usermod_st7565)
