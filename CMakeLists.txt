zephyr_include_directories(include)

if(CONFIG_DYA_OLED_STATUS_WIDGET)
  target_sources(app PRIVATE src/runtime_config.c)
endif()

if(CONFIG_DYA_OLED_STATUS_STUDIO_RPC)
  target_sources(app PRIVATE src/studio/oled_status_handler.c)

  list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
  include(nanopb)
  set(NANOPB_GENERATE_CPP_APPEND_PATH TRUE)
  set(NANOPB_GENERATE_CPP_STANDALONE OFF)

  zephyr_library_named(dya_oled_status_proto)
  set(PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/proto/dya/oled_status/oled_status.proto)

  nanopb_generate_cpp(proto_srcs proto_hdrs RELPATH ${CMAKE_CURRENT_SOURCE_DIR} ${PROTO_FILES})
  target_include_directories(dya_oled_status_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
  target_sources(dya_oled_status_proto PRIVATE ${proto_srcs} ${proto_hdrs})

  target_include_directories(app PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/proto)
  add_dependencies(app dya_oled_status_proto)
endif()
