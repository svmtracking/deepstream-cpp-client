#################################
#### Target: dsclientTest  ####
#################################
include_directories(   "${CMAKE_CURRENT_SOURCE_DIR}/../src/PhTranslateLib"
                        ${CMAKE_CURRENT_BINARY_DIR})
						
ADD_EXECUTABLE(dsclientTest dsclient/main.cpp)
target_link_libraries(dsclientTest  dscppclient ${LIBUV_LIBRARY})
if (UNIX)
	target_link_libraries (dsclientTest LINK_PUBLIC pthread)
endif()
set_target_properties(dsclientTest PROPERTIES 
								COMPILE_DEFINITIONS "${TARGET_COMPILE_DEFS};"
								COMPILE_FLAGS "${TARGET_COMPILE_FLAGS}")