target_sources(syntropic INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/syn_ao.c
    ${CMAKE_CURRENT_SOURCE_DIR}/syn_event_flags.c
    ${CMAKE_CURRENT_SOURCE_DIR}/syn_mutex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/syn_sched.c
    ${CMAKE_CURRENT_SOURCE_DIR}/syn_sequencer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/syn_timer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/syn_timer_wheel.c
    ${CMAKE_CURRENT_SOURCE_DIR}/syn_watchdog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/syn_workqueue.c
)
