# deepstream-cpp-client 
# Copyright (c) 2016 Cenacle Research

cmake_minimum_required(VERSION 3.0)

PROJECT( dscppclient )

#################################
# Project Info
#################################
SET(PROJECT_NAME_SHORT   "dscppclient")
SET(PROJECT_NAME_LONG    "DeepStream C++ Client")
SET(PROJECT_DESCRIPTION  "C++ client for DeepStream.io")
SET(PROJECT_COPYRIGHT    "Copyright (C) 2016 Cenacle Research")
SET(PROJECT_CONTACT      "http://cenacleresearch.com/#contact")
SET(PROJECT_VENDOR       "Cenacle Research")
SET(ORG_WEBSITE          "http://cenacleresearch.com/")

#################################
# Build Options
#################################

SET (BUILD_UNICODE ON CACHE BOOL "dscppclient: Should be built with Unicode? (recommended: Yes)" FORCE)
OPTION (BUILD_DSCPPCLIENT_TESTS "dscppclient: Should build tests?" ON)


#################################
# Output options
#################################
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/")
include (build_options)

set_output_directories()
set_output_postfix()
set_compiler_options()

#################################
#### Target: dependencies  ####
#################################
find_path(LIBUV_INCLUDE_DIR uv.h)
find_library(LIBUV_LIBRARY NAMES uv uv1)

# ADD_SUBDIRECTORY(src/3rdparty/cpp.react)
ADD_SUBDIRECTORY(src/3rdparty/g3log)

#################################
#### Target: dscppclient  ####
#################################
set(SrcDir ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(CedarDir ${SrcDir}/3rdparty/cedar)

SET(DSCPPClient_INCLUDE_DIRS 
		${CMAKE_CURRENT_SOURCE_DIR}/include 
		${SrcDir}
		${CedarDir}
		${LIBUV_INCLUDE_DIR})

include_directories(${DSCPPClient_INCLUDE_DIRS})
						
SET(DSCPPClient_HEADERS	
	${CedarDir}/cedarpp.h
	${SrcDir}/bufPool.h
	${SrcDir}/trie_array.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/dsclientbase.h)
SET(DSCPPClient_SOURCES 
	${SrcDir}/libuv-network.cpp)

ADD_LIBRARY(dscppclient STATIC ${DSCPPClient_HEADERS} ${DSCPPClient_SOURCES})

set_target_properties(dscppclient PROPERTIES 
							COMPILE_DEFINITIONS "${TARGET_COMPILE_DEFS}"
							COMPILE_FLAGS "${TARGET_COMPILE_FLAGS}")

if (UNIX)
	set (DSCPPClient_Dependencies LINK_PUBLIC pthread ${LIBUV_LIBRARY})
endif()
if (MSVC)
	set (DSCPPClient_Dependencies LINK_PUBLIC ws2_32.lib Psapi.lib IPHLPAPI.lib Userenv.lib ${LIBUV_LIBRARY})
endif()

target_link_libraries(dscppclient ${DSCPPClient_Dependencies})

SET(DSCPPClient_LIBRARIES dscppclient ${DSCPPClient_Dependencies})

install(TARGETS dscppclient RUNTIME DESTINATION bin COMPONENT HelperApps  
							LIBRARY DESTINATION bin COMPONENT HelperApps  
							ARCHIVE DESTINATION lib	COMPONENT HelperApps  )

#################################
#### Target: dscppclient tests  #
#################################
if(BUILD_DSCPPCLIENT_TESTS)
  ADD_SUBDIRECTORY(test)
endif()