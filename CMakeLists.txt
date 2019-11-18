cmake_minimum_required(VERSION 3.1)

project(SentryCpp)
set(PROJECT_NAME SentryCpp)

option(DEBUG_SENTRY "DEBUG_SENTRY" OFF)
#option(TEST_SENTRY "TEST_SENTRY_ENABLED" OFF)


set(CMAKE_CXX_STANDARD 17)

set(SOURCES
    "include/sentry.h"
    "src/sentry.cpp"
    "include/sentry_common.h"
    "src/sentry_common.cpp"
    "src/transport.h"
    "src/transport.cpp"
    "src/scope.h"
    "src/scope.cpp"
    "src/hub.h"
    "src/hub.cpp"
    "src/backtracehandler.cpp"
    "src/backtracehandler.h"
    )

add_library(${PROJECT_NAME} ${SOURCES})

target_compile_options(${PROJECT_NAME} PRIVATE -DUSE_STANDALONE_ASIO -DASIO_STANDALONE -Wall -Wextra -pedantic)

set(asio_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/asio-1.12.1/include" CACHE INTERNAL "asio library includes" FORCE)
set(asio_HEADERS_ONLY TRUE CACHE INTERNAL "No target generated for asio" FORCE)

target_include_directories(${PROJECT_NAME}
                        PUBLIC
                            $<INSTALL_INTERFACE:include>
                            $<INSTALL_INTERFACE:externals/asio-1.12.2/include>
                            $<INSTALL_INTERFACE:externals/json-3.1.2>
                            $<INSTALL_INTERFACE:externals/http-server-3.0.0>
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/externals/json-3.1.2>
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/externals/http-server-3.0.0>
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/externals/asio-1.12.2/include>
                        PRIVATE
                            ${CMAKE_CURRENT_SOURCE_DIR}/src
                            )

target_link_libraries(${PROJECT_NAME} pthread uuid bfd)

message("DEBUG_SENTRY = ${DEBUG_SENTRY}")
if (${DEBUG_SENTRY})
    add_definitions(-DDEBUG_SENTRYCPP)
endif()

#if (${TEST_SENTRY})
#    add_subdirectory(test)
#endif()

include(CMakePackageConfigHelpers)

set(INSTALL_CONFIGDIR cmake)

message("Installation path: " ${CMAKE_INSTALL_PREFIX})

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/SentryCppConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/SentryCppConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
    )

export(TARGETS ${PROJECT_NAME} NAMESPACE SentryCpp::
    FILE ${PROJECT_BINARY_DIR}/SentryCppTargets.cmake)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/SentryCppConfig.cmake
        DESTINATION  ${INSTALL_CONFIGDIR} )

install(EXPORT SentryCppTargets
        DESTINATION ${INSTALL_CONFIGDIR}
        NAMESPACE SentryCpp:: )

install(TARGETS ${PROJECT_NAME} EXPORT SentryCppTargets DESTINATION lib)
install(FILES ${PROJECT_SOURCE_DIR}/include/sentry.h
       DESTINATION include)
install(FILES ${PROJECT_SOURCE_DIR}/include/sentry_common.h
      DESTINATION include)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/externals/ DESTINATION externals)

