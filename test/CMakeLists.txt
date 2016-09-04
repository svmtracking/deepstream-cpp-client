#################################
#### Target: dsclientTest  ####
#################################
include_directories(   ${DSCPPClient_INCLUDE_DIRS}
                        ${CMAKE_CURRENT_BINARY_DIR})
						
ADD_EXECUTABLE(dsclientTest dsclient/main.cpp)
target_link_libraries(dsclientTest  dscppclient)
set_target_properties(dsclientTest PROPERTIES 
								COMPILE_DEFINITIONS "${TARGET_COMPILE_DEFS};"
								COMPILE_FLAGS "${TARGET_COMPILE_FLAGS}")