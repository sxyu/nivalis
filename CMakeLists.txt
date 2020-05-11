cmake_minimum_required( VERSION 2.8 )

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
if (POLICY CMP0048)
    cmake_policy(SET CMP0048 NEW)
endif (POLICY CMP0048)

project( nivalis )

set( _VERSION_ "0.0.3 alpha" )
set( _COPYRIGHT_ "Alex Yu 2020" )

set( CMAKE_CXX_STACK_SIZE "10000000" )
set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON)
set( CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" )

option ( USE_OPENGL_IMGUI "Use OpenGL3/GLFW/ImGui GUI backend" ON )
option ( USE_NANA "Use Nana GUI backend (fallback if ImGui config fails)" ON )
option ( USE_BOOST_MATH "Use Boost math function (e.g. gamma, digamma, zeta, beta)" ON )
option ( USE_SYSTEM_GLFW "Use system glfw3 if available" ON )

if (WIN32)
    set( COLOR_TERM OFF )
else()
    set( COLOR_TERM ON )
endif()

if( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE Release )
endif()

set( LIB_PROJ_NAME "libnivalis" )
set( SHELL_PROJ_NAME "nivalis" )
set( PLOT_GUI_PROJ_NAME "nivplot" )
set( INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include" )

if ( CMAKE_COMPILER_IS_GNUCXX )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated -Wno-deprecated-declarations -O3 -g" )
endif ( CMAKE_COMPILER_IS_GNUCXX )

set(
    PROJ_EXECUTABLES
    ${SHELL_PROJ_NAME}
)

set(
  HEADERS
  version.hpp
  parser.hpp
  env.hpp
  expr.hpp
  ast_nodes.hpp
  util.hpp
  eval_expr.hpp
  opcodes.hpp
  diff_expr.hpp
  plotter/plotter.hpp
  plotter/plot_gui.hpp
)
list(TRANSFORM HEADERS PREPEND ${INCLUDE_DIR}/)

set(
  SOURCES
  parser.cpp
  env.cpp
  expr.cpp
  ast_nodes.cpp
  util.cpp
  eval_expr.cpp
  opcodes.cpp
  diff_expr.cpp
  plotter/common.cpp
)

include_directories(
    ${INCLUDE_DIR}
    ${X11_INCLUDE_DIR}
    ${Fontconfig_INCLUDE_DIRS}
    ${NANA_INCLUDE_DIR}
)

set (PROJ_DEPENDENCIES )

set ( _BOOST_ENABLED_ "//" )
set ( BOOST_ENABLED OFF )
if ( USE_BOOST_MATH )
    set( Boost_USE_STATIC_LIBS ON )
    set( Boost_USE_STATIC ON )
    find_package(Boost)
    if (Boost_FOUND)
        set ( _BOOST_ENABLED_ "" )
        set ( BOOST_ENABLED ON )
        message ( STATUS "Boost math found, enabled" )
    else()
        message ( WARNING "Boost math not found, some functions (beta, digamma,...) may not be available" )
    endif()
else ()
    message ( STATUS "Boost math was disabled by user" )
endif ()


find_package(Threads REQUIRED)

set ( _NANA_ENABLED_ "//" )
set ( NANA_ENABLED OFF )
set ( _OPENGL_IMGUI_ENABLED_ "//" )
set ( OPENGL_IMGUI_ENABLED OFF )

if ( USE_OPENGL_IMGUI )
    set ( IMGUI_DIR "${PROJECT_SOURCE_DIR}/3rdparty/imgui" )
    set ( GLFW_DIR "${PROJECT_SOURCE_DIR}/3rdparty/glfw3" )
    set ( WILL_USE_SYSTEM_GLFW ${USE_SYSTEM_GLFW} )

    find_package(PkgConfig)
    if ( NOT PkgConfig_FOUND )
        set ( WILL_USE_SYSTEM_GLFW OFF )
    else()
        pkg_check_modules(GLFW glfw3)
        if ( NOT GLFW_FOUND )
            set ( WILL_USE_SYSTEM_GLFW OFF )
        endif ()
    endif ()

    if ( NOT WILL_USE_SYSTEM_GLFW )
        message ( STATUS "Using included glfw3 (in 3rdparty/)" )
        add_subdirectory( "${GLFW_DIR}" )
        set( PROJ_DEPENDENCIES
            ${PROJ_DEPENDENCIES} glfw)
        include_directories( "${GLFW_DIR}/include" )
    else()
        message ( STATUS "Using system glfw3" )
    endif ()

    find_package(OpenGL REQUIRED)
    find_package(GLEW REQUIRED)
    add_definitions(-DGLEW_STATIC)

    include_directories(${GLEW_INCLUDE_DIRS})

    set ( _OPENGL_IMGUI_ENABLED_ "" )
    set ( OPENGL_IMGUI_ENABLED ON )
    set( PROJ_DEPENDENCIES
        OpenGL::GL
        ${PROJ_DEPENDENCIES} ${GLFW_STATIC_LIBRARIES}
        ${GLEW_LIBRARIES}
        )

    set ( SOURCES ${SOURCES}
        "${IMGUI_DIR}/imgui.cpp"
        "${IMGUI_DIR}/imgui_draw.cpp"
        "${IMGUI_DIR}/imgui_widgets.cpp"
        "plotter/opengl_imgui_backend.cpp"
    )
    set ( SOURCES ${SOURCES}
        "${IMGUI_DIR}/imgui_impl_opengl3.cpp"
        "${IMGUI_DIR}/imgui_impl_glfw.cpp"
        )
    include_directories( "${IMGUI_DIR}")
        
    message ( STATUS "Using OpenGL with ImGui as plotter backend" )
else()
    message ( STATUS "OpenGL + ImGui plotter backend was disabled by user" )
endif()

if ( (USE_NANA) AND (NOT OPENGL_IMGUI_ENABLED) )
    IF (NOT WIN32)
        find_package(X11)
        find_package(Fontconfig)
        find_package(JPEG)
    endif()
    set (SOURCES ${SOURCES} "plotter/nana_backend.cpp")
    find_package(Nana)
    if (NANA_FOUND)
        set ( _NANA_ENABLED_ "" )
        set ( NANA_ENABLED ON )
        message ( STATUS "Nana found, using as plotter backend" )
    else ()
        message ( WARNING "Nana not found, Nana plotter backend disabled" )
    endif ()
elseif ( NOT USE_NANA )
    message ( STATUS "Nana was disabled by user" )
endif ()

if ( (NANA_ENABLED) OR (OPENGL_IMGUI_ENABLED) )
    set( PROJ_EXECUTABLES ${PROJ_EXECUTABLES} ${PLOT_GUI_PROJ_NAME} )
else()
    message ( WARNING "No usable GUI backend found, no GUI will be built...")
endif ()

configure_file( version.hpp.in ${INCLUDE_DIR}/version.hpp )

set (NANA_DEPENDENCIES
    ${NANA_LIBRARIES}
    ${X11_X11_LIB}
    ${X11_Xft_LIB}
    ${Fontconfig_LIBRARIES}
    ${JPEG_LIBRARIES}
)

if ( BOOST_ENABLED )
    include_directories( ${Boost_INCLUDE_DIRS} )
    set (PROJ_DEPENDENCIES ${PROJ_DEPENDENCIES} )
# ensure boost doesn't throw exception when evaluating out of domain
add_definitions( -DBOOST_MATH_DOMAIN_ERROR_POLICY=ignore_error
    -DBOOST_MATH_POLE_ERROR_POLICY=ignore_error -DBOOST_MATH_OVERFLOW_ERROR_POLICY=ignore_error
    -DBOOST_MATH_UNDERFLOW_ERROR_POLICY=ignore_error ) 
if ( MSVC )
    add_definitions(-D_HAS_AUTO_PTR_ETC=1)
endif ()
endif ()

if ( NANA_ENABLED )
set (PROJ_DEPENDENCIES ${NANA_DEPENDENCIES} ${PROJ_DEPENDENCIES} )
endif ()

add_definitions( -D_USE_MATH_DEFINES ) 

add_library( ${LIB_PROJ_NAME} STATIC ${HEADERS} ${SOURCES} )
set_target_properties(${LIB_PROJ_NAME} PROPERTIES OUTPUT_NAME 
    ${SHELL_PROJ_NAME} )

foreach(targ ${PROJ_EXECUTABLES})
    add_executable( ${targ} main_${targ}.cpp )
    target_link_libraries( ${targ}
        ${LIB_PROJ_NAME}
        ${CMAKE_THREAD_LIBS_INIT}
        ${PROJ_DEPENDENCIES} )
    
    if ( MSVC )
        set_property(TARGET ${targ} APPEND PROPERTY LINK_FLAGS "/DEBUG /LTCG")
        add_definitions(-D_CRT_SECURE_NO_WARNINGS)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT /GLT")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    endif ( MSVC )
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_link_libraries( ${targ} stdc++fs )
    endif ()
endforeach()
# Create source group for headers
source_group( "Header Files" FILES ${HEADERS} )
