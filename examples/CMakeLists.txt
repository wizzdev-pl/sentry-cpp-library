cmake_minimum_required(VERSION 3.2)

project(test_sentry)
set(CMAKE_CXX_STANDARD 17)

add_executable(${PROJECT_NAME} "main.cpp")


## with add_subdirectory
add_subdirectory(externals/SentryCpp)
# or:
#add_subdirectory(${CMAKE_SOURCE_DIR}/externals/SentryCpp
#	${CMAKE_BINARY_DIR}/externals/SentryCpp)

target_link_libraries(${PROJECT_NAME} PUBLIC SentryCpp)



## with find_package
#set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR}/SentryCpp)
#find_package(SentryCpp REQUIRED)

#if (SentryCpp_FOUND)
#    message(" ii ${SentryCpp_INCLUDE_DIRS}")

#    target_link_libraries(test_sentry SentryCpp::SentryCpp)

##    include_directories(
##        ${SentryCpp_DIR}/../externals/json-3.1.2
##        ${SentryCpp_DIR}/../externals/asio-1.12.2/include
##        ${SentryCpp_DIR}/../externals/http-server-3.0.0
##        ${SentryCpp_DIR}/../src
##        )

#endif()
