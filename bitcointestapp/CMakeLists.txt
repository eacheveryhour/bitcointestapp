cmake_minimum_required(VERSION 2.8)

project(test)
link_directories(${PROJECT_SOURCE_DIR}/lib)

FILE(GLOB_RECURSE INC_SRC "src/*.*")
include_directories("include")
include_directories("src")

find_library(PTHREAD pthread)
message(STATUS "Found " ${PTHREAD})
find_library(GMP gmp)
message(STATUS "Found " ${GMP})
find_library(JSON_CPP jsoncpp)
message(STATUS "Found " ${JSON_CPP})
find_library(JSON_RPC_CPP_CLIENT jsonrpccpp-client)
message(STATUS "Found " ${JSON_RPC_CPP_CLIENT})
find_library(JSON_RPC_CPP_COMMON jsonrpccpp-common)
message(STATUS "Found " ${JSON_RPC_CPP_COMMON})
find_library(JSON_RPC_CPP_SERVER jsonrpccpp-server)
message(STATUS "Found " ${JSON_RPC_CPP_SERVER})

add_executable(${PROJECT_NAME} "main.cpp" ${INC_SRC})

target_link_libraries(${PROJECT_NAME} libleveldb.a)
target_link_libraries(${PROJECT_NAME} libbtc.a)
target_link_libraries(${PROJECT_NAME} libsecp256k1.a)

target_link_libraries(${PROJECT_NAME} ${GMP})
target_link_libraries(${PROJECT_NAME} ${JSON_CPP})
target_link_libraries(${PROJECT_NAME} ${JSON_RPC_CPP_COMMON})
target_link_libraries(${PROJECT_NAME} ${JSON_RPC_CPP_CLIENT})
target_link_libraries(${PROJECT_NAME} ${JSON_RPC_CPP_SERVER})
target_link_libraries(${PROJECT_NAME} ${PTHREAD})

