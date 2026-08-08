target_sources(syntropic INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/syn_biquad.c
    ${CMAKE_CURRENT_SOURCE_DIR}/syn_dds.c
    ${CMAKE_CURRENT_SOURCE_DIR}/syn_fft.c
    ${CMAKE_CURRENT_SOURCE_DIR}/syn_filter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/syn_filter_design.c
    ${CMAKE_CURRENT_SOURCE_DIR}/syn_kalman.c
    ${CMAKE_CURRENT_SOURCE_DIR}/syn_signal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/syn_goertzel.c
)
