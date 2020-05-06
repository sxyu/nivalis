cmake_minimum_required( VERSION 2.8 )

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

project( nivalis )

set( CMAKE_CXX_STACK_SIZE "10000000" )
set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON)
set( CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" )
if (WIN32)
    set( COLOR_TERM OFF )
else()
    set( COLOR_TERM ON )
endif()

if( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE Release )
endif()

set( PROJ_NAME "nivalis" )
set( OUTPUT_NAME "nivalis" )
set( INCLUDE_DIR "include" )

if ( CMAKE_COMPILER_IS_GNUCXX )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated -Wno-deprecated-declarations -O3 -g" )
endif ( CMAKE_COMPILER_IS_GNUCXX )

# set( Boost_USE_STATIC_LIBS ON )
# set( Boost_USE_STATIC ON )

# find_package(Boost REQUIRED COMPONENTS system program_options filesystem )
find_package(Threads REQUIRED)
find_package(X11 REQUIRED)
# find_package(Freetype REQUIRED)
find_package(Fontconfig REQUIRED)
find_package(JPEG REQUIRED)
find_package(Nana REQUIRED)
include_directories(
    ${INCLUDE_DIR}
    # ${Boost_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR}
    # ${FREETYPE_INCLUDE_DIRS}
    ${Fontconfig_INCLUDE_DIRS}
    ${NANA_INCLUDE_DIR}
)

set(
  HEADERS
  parser.hpp
  plotgui.hpp
  env.hpp
  expr.hpp
  util.hpp
)
list(TRANSFORM HEADERS PREPEND ${INCLUDE_DIR}/)

set(
  SOURCES
  main.cpp
  parser.cpp
  plotgui.cpp
  env.cpp
  expr.cpp
  util.cpp
)

set (NANA_LINK_LIBS
    ${X11_X11_LIB}
    ${X11_Xft_LIB}
    ${CMAKE_THREAD_LIBS_INIT}
    ${Fontconfig_LIBRARIES}
    ${JPEG_LIBRARIES}
)

add_executable( ${PROJ_NAME} ${HEADERS} ${SOURCES} )
set_target_properties( ${PROJ_NAME} PROPERTIES OUTPUT_NAME ${OUTPUT_NAME} )
target_link_libraries( ${PROJ_NAME}  #${Boost_LIBRARIES}
     ${NANA_LIBRARIES} ${NANA_LINK_LIBS} )

if ( MSVC )
    set_property(TARGET ${PROJ_NAME} APPEND PROPERTY LINK_FLAGS /DEBUG)
endif ( MSVC )
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_link_libraries( ${PROJ_NAME} stdc++fs )
endif ()
# Create source group for headers
source_group( "Header Files" FILES ${HEADERS} )
