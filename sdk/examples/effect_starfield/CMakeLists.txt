# Example LUMAFLOW effect plugin (C ABI, no Qt dependency).
add_library(lumaflow_starfield MODULE starfield.c)
target_include_directories(lumaflow_starfield PRIVATE ${CMAKE_SOURCE_DIR}/sdk/include)
set_target_properties(lumaflow_starfield PROPERTIES PREFIX "" C_VISIBILITY_PRESET hidden)
install(TARGETS lumaflow_starfield LIBRARY DESTINATION lib/lumaflow/plugins RUNTIME DESTINATION lib/lumaflow/plugins)
