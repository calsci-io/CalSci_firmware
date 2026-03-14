##############################################################################
# hybrid_sim user-module build rules
##############################################################################
add_library(usermod_hybrid_sim INTERFACE)

target_sources(usermod_hybrid_sim INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/hybrid_sim_capture.c
    ${CMAKE_CURRENT_LIST_DIR}/modhybrid_sim.c
)

target_include_directories(usermod_hybrid_sim INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(usermod INTERFACE usermod_hybrid_sim)
