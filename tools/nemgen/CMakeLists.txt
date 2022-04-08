cmake_minimum_required(VERSION 3.14)

add_subdirectory(blockhashes)

catapult_define_tool(nemgen)

target_include_directories(catapult.tools.nemgen PRIVATE "${CMAKE_SOURCE_DIR}/src/")

target_link_libraries(catapult.tools.nemgen
	catapult.tools.nemgen.blockhashes
	catapult.tools.plugins
	catapult.finalization
	catapult.plugins.coresystem
	catapult.model
	catapult.plugins.mosaic
	catapult.plugins.namespace
	catapult.plugins.transfer)
