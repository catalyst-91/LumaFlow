add_library(lumaflow_framegrab MODULE framegrab.c)
target_include_directories(lumaflow_framegrab PRIVATE ${CMAKE_SOURCE_DIR}/sdk/include)
set_target_properties(lumaflow_framegrab PROPERTIES PREFIX "" C_VISIBILITY_PRESET hidden LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sdk/examples/outputs)
install(TARGETS lumaflow_framegrab LIBRARY DESTINATION lib/lumaflow/plugins RUNTIME DESTINATION lib/lumaflow/plugins)
