# initialize the variables defining output directories
#
# Sets the following variables:
#
# - :cmake:data:`CMAKE_ARCHIVE_OUTPUT_DIRECTORY`
# - :cmake:data:`CMAKE_LIBRARY_OUTPUT_DIRECTORY`
# - :cmake:data:`CMAKE_RUNTIME_OUTPUT_DIRECTORY`
#
# plus the per-config variants, ``*_$<CONFIG>``
#
# @public
#
macro(set_output_directories)

  set(BUILD_TARGET_ROOT ${CMAKE_SOURCE_DIR} CACHE PATH "The root folder that contains bin and lib directories")

  # Directory for output files
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BUILD_TARGET_ROOT}/lib 
    CACHE PATH "Output directory for static libraries.")

  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BUILD_TARGET_ROOT}/lib
    CACHE PATH "Output directory for shared libraries.")

  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BUILD_TARGET_ROOT}/bin
    CACHE PATH "Output directory for executables and DLL's.")

  if(NOT CMAKE_CONFIGURATION_TYPES)
     set (CMAKE_CONFIGURATION_TYPES "Debug;Release;MinSizeRel;RelWithDebInfo" CACHE STRING
     "Semicolon separated list of supported configuration types, only supports Debug, Release, MinSizeRel, and RelWithDebInfo, anything else will be ignored.")
     mark_as_advanced(CMAKE_CONFIGURATION_TYPES)
  endif()    

  foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${BUILD_TARGET_ROOT}/bin" CACHE PATH "" FORCE)
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${BUILD_TARGET_ROOT}/lib" CACHE PATH "" FORCE)
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${BUILD_TARGET_ROOT}/lib" CACHE PATH "" FORCE)
    mark_as_advanced(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG})
    mark_as_advanced(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG})
    mark_as_advanced(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG})    
  endforeach()  

endmacro()

#################################
# set output prefixes
#################################
macro(set_output_postfix)
  set(Char_Flags "" CACHE INTERNAL "Compiler Character Flags: Unicode/MBCS Options")
  set(Char_postfix "" CACHE INTERNAL "Unicode Postfix")
  set(Output_postfix "" CACHE INTERNAL "Debug Postfix") 
          
  if(BUILD_UNICODE)
      SET(Char_postfix "")
      SET(Char_Flags "UNICODE;_UNICODE;")
      message(STATUS "${PROJECT_NAME}: Unicode build is ON")
  else()
      SET(Char_postfix "mb")
      SET(Char_Flags "_MBCS;")
      message(STATUS "${PROJECT_NAME}: Unicode build is OFF")
  endif()

  IF(NOT CMAKE_BUILD_TYPE)
      SET(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
          "Choose the type of build, options are: Debug Release RelWithDebInfo"
          FORCE)
  ENDIF()

  if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
      SET(Output_postfix "d")
  else()
      SET(Output_postfix "")
  endif()   
          
  SET(CMAKE_DEBUG_POSTFIX "d${Char_postfix}")
  SET(CMAKE_RELEASE_POSTFIX "${Char_postfix}")
  
  SET(TARGET_COMPILE_DEFS "${Char_Flags};_CRT_SECURE_NO_DEPRECATE;_CRT_SECURE_NO_WARNINGS;_BIND_TO_CURRENT_CRT_VERSION;")  

  message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
  message(STATUS "Output Postfix: ${Output_postfix}${Char_postfix}")
endmacro()


#################################
# Set Compiler options 
#################################
macro(set_compiler_options)
  IF(CMAKE_COMPILER_IS_GNUCC)
    SET(TARGET_COMPILE_FLAGS "-std=gnu++0x" ) # Add C++0x support for GCC
  ENDIF()
    
  IF(MSVC)
    SET(TARGET_COMPILE_FLAGS "${TARGET_COMPILE_FLAGS} /Zc:wchar_t- /Zc:forScope")
  ENDIF(MSVC)
    
  if(QT4_FOUND)
    INCLUDE(${QT_USE_FILE})
  endif()

  # Use relative paths, to reduce command-line length
  if(WIN32)
    set(CMAKE_USE_RELATIVE_PATHS true)
    set(CMAKE_SUPPRESS_REGENERATION true)
  endif()  
endmacro()

#################################
# Set Q5 Options 
#################################
macro(set_qt5_options)

  #----- Instruct CMake to run moc automatically when needed -----
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_AUTOUIC ON)
  set(CMAKE_INCLUDE_CURRENT_DIR ON)  
  
  find_package(Qt5Widgets REQUIRED)
endmacro()