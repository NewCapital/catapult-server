cmake_minimum_required(VERSION 3.14)

catapult_library_target(catapult.model)
target_link_libraries(catapult.model catapult.crypto catapult.cache_db)

target_include_directories(catapult.model PUBLIC "${CMAKE_SOURCE_DIR}/src/catapult/cache_db")