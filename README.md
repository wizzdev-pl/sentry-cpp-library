# SentryCpp v0.1
Sentry client library for C++ 

the newest version of the source code can be found on [Wizzdev's github page](https://github.com/wizzdev-pl/sentry-cpp-library)  .

## Requirements:

(in externals/)

   - http-server-3.0.0
   - asio-1.12.2
   - json-3.1.2



### To integrate with upstream project (using CMake):


#### Option #1 (recomended) compiling the library with upstream project

Provided that SentryCpp is in the subdirectory `externals/` located in the root directory of the current project, insert this two lines into top level CMakeLists.txt:

    add_subdirectory(externals/SentryCpp)
    target_link_libraries(${PROJECT_NAME} PUBLIC SentryCpp)



#### Option #2 - precompiling the library and using find_package()

- build and install the library (using Cmake and preferably specifying CMAKE_INSTALL_PEREFIX to a subdirectory of the target project build directory):
      
      cmake path_to_SentryCpp_source_dir -DCMAKE_INSTALL_PREFIX:PATH=install_path
      make -j`grep -c ^processor /proc/cpuinfo`
      make install

the installation will create 3 directories in the destination folder:
      
- **lib/** with *libSentryCpp.a* library file
- **include/** with *sentry.h* public header file 
- **externals/** with copy of all external library dependencies to be included with upstream project


- in the target project CMake file:
	
	- set CMAKE_PREFIX_PATH to SentryCpp install directory
	- find_package SentryCpp
	- include directories from /externals/ (installed along with the SentryCpp library) 
		
	
example CMake:

	set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR}/SentryCpp)
	find_package(SentryCpp REQUIRED)

	if (SentryCpp_FOUND)

	    target_link_libraries(test_sentry SentryCpp::SentryCpp)

	    include_directories(
		${SentryCpp_DIR}/../externals/json-3.1.2
		${SentryCpp_DIR}/../externals/asio-1.12.2/include
		${SentryCpp_DIR}/../externals/http-server-3.0.0
		${SentryCpp_DIR}/../src
		)

	endif()



## Adding Sentry to project
    
     #include "sentry.h"


    // to initialise library a unique client key (SENTRY_DSN) is required:

    std::string dsn = "https://example_key@sentry.io/project_ID"
    
    Sentry::SentryOptions initSentryParameters =  Sentry::SentryOptions();
    initSentryParameters.dsn = dsn;

    Sentry::init(initSentryParameters);


## Using SentryCpp

All unhandled exceptions and terminating signals will be automatically reported.
To set custom tags for all events:

	Sentry::setTag("key", "value");


To add/report custom event use:


	json event;
	event["message"] = "Custom event";
	event["level"] = "warning";

	Sentry::captureEvent(event);

To add custom breadCrumb:

	Sentry::addBreadcrumb(breadcrumb);

(also as json object)

A convenient function `log()` is provided to enable easy integration with application specific loggers - all logs will be stored as breadcrumbs and those with level above WARNING will be reported to Sentry immediately:
 	
 	Sentry::log(Sentry::EventLevel::LEVEL_ERROR, "This is an error!");

