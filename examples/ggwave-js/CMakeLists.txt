set(TARGET ggwave-js)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/index-tmpl.html       ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/index.html @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/bindings/javascript/ggwave.js ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/ggwave.js  COPYONLY)
