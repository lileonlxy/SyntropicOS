# BLE Host Stack CMakeLists.txt

target_sources(syntropic PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/syn_ble_hci.c
    ${CMAKE_CURRENT_LIST_DIR}/syn_ble_l2cap.c
    ${CMAKE_CURRENT_LIST_DIR}/syn_ble_att.c
    ${CMAKE_CURRENT_LIST_DIR}/syn_ble_gap.c
    ${CMAKE_CURRENT_LIST_DIR}/syn_ble_gatt.c
)

target_include_directories(syntropic PUBLIC ${CMAKE_CURRENT_LIST_DIR})
