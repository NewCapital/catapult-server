cmake_minimum_required(VERSION 3.14)

catapult_define_tool(verify)
target_include_directories(catapult.tools.verify PRIVATE "${CMAKE_SOURCE_DIR}/src/")
target_link_libraries(catapult.tools.verify catapult.tools.plugins catapult.finalization catapult.model)
