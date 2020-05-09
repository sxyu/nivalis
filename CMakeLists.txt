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

set( Boost_USE_STATIC_LIBS ON )
set( Boost_USE_STATIC ON )

find_package(Boost REQUIRED COMPONENTS system program_options filesystem )

find_package(Threads REQUIRED)
IF (NOT WIN32)
    find_package(X11 REQUIRED)
    find_package(Fontconfig REQUIRED)
    find_package(JPEG REQUIRED)
endif()
find_package(Nana REQUIRED)
include_directories(
    ${INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR}
    ${Fontconfig_INCLUDE_DIRS}
    ${NANA_INCLUDE_DIR}
)

set(
  HEADERS
  parser.hpp
  plotgui.hpp
  env.hpp
  expr.hpp
  ast_nodes.hpp
  util.hpp
  eval_expr.hpp
  opcodes.hpp
  diff_expr.hpp
)
list(TRANSFORM HEADERS PREPEND ${INCLUDE_DIR}/)

set(
  SOURCES
  main.cpp
  parser.cpp
  plotgui.cpp
  env.cpp
  expr.cpp
  ast_nodes.cpp
  util.cpp
  eval_expr.cpp
  opcodes.cpp
  diff_expr.cpp
)

set (NANA_DEPENDENCIES
    ${X11_X11_LIB}
    ${X11_Xft_LIB}
    ${CMAKE_THREAD_LIBS_INIT}
    ${Fontconfig_LIBRARIES}
    ${JPEG_LIBRARIES}
)
# Ensure M_PI, M_E defined; ensure boost doesn't throw exception when evaluating out of domain
add_definitions( -D_USE_MATH_DEFINES -DBOOST_MATH_DOMAIN_ERROR_POLICY=ignore_error
    -DBOOST_MATH_POLE_ERROR_POLICY=ignore_error -DBOOST_MATH_OVERFLOW_ERROR_POLICY=ignore_error
    -DBOOST_MATH_UNDERFLOW_ERROR_POLICY=ignore_error ) 

add_executable( ${PROJ_NAME} ${HEADERS} ${SOURCES} )
set_target_properties( ${PROJ_NAME} PROPERTIES OUTPUT_NAME ${OUTPUT_NAME} )
target_link_libraries( ${PROJ_NAME}  ${Boost_LIBRARIES} ${NANA_LIBRARIES} ${NANA_DEPENDENCIES} )

if ( MSVC )
    # Extra flags for MSVC to stop it from complaining about LTCG, MT etc.
    set_property(TARGET ${PROJ_NAME} APPEND PROPERTY LINK_FLAGS "/DEBUG /LTCG")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT /GL")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
endif ( MSVC )
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_link_libraries( ${PROJ_NAME} stdc++fs )
endif ()
# Create source group for headers
source_group( "Header Files" FILES ${HEADERS} )
