cmake_minimum_required( VERSION 2.8 )

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
if (POLICY CMP0048)
    cmake_policy(SET CMP0048 NEW)
endif (POLICY CMP0048)

project( nivalisc )

set( _VERSION_ "c0.1.0a-5 (alpha)" )
set( _COPYRIGHT_ "Alex Yu 2020" )

set( CMAKE_CXX_STACK_SIZE "10000000" )
set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON)
set( CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" )

option ( USE_SYSTEM_GLFW "Use system glfw3 if available" ON )

if( NOT CMAKE_BUILD_TYPE )
    set( CMAKE_BUILD_TYPE Release )
endif()

set( LIB_PROJ_NAME "libnivalisc" )
set( SHELL_PROJ_NAME "nivalisc" )
set( PLOT_GUI_PROJ_NAME "nivplotc" )
set( INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include" )
set( SRC_DIR "${PROJECT_SOURCE_DIR}/src" )

set ( IMGUI_DIR "3rdparty/imgui" )
set ( GLEW_DIR "3rdparty/glew" )
set ( GLFW_DIR "3rdparty/glfw3" )
set ( INCLUDE_DIR_3RDPARTY "3rdparty/include" )

if ( CMAKE_COMPILER_IS_GNUCXX )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated -Wno-deprecated-declarations -g" )
endif ( CMAKE_COMPILER_IS_GNUCXX )

# Git submodule auto update
# https://cliutils.gitlab.io/modern-cmake/chapters/projects/submodule.html
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    # Update submodules as needed
    option(GIT_SUBMODULE "Check submodules during build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()
endif()

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/${GLFW_DIR}/CMakeLists.txt")
   message(FATAL_ERROR "A submodule as not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
endif()

set(
    PROJ_EXECUTABLES
    ${PLOT_GUI_PROJ_NAME}
    ${SHELL_PROJ_NAME}
)

set(
    HEADERS
    parser.hpp
    env.hpp
    expr.hpp
    util.hpp
    opcodes.hpp
    shell.hpp
    json.hpp
    color.hpp
    point.hpp
    plotter/plotter.hpp
    plotter/internal.hpp
    plotter/imgui_adaptor.hpp
)
list(TRANSFORM HEADERS PREPEND ${INCLUDE_DIR}/)

set(
    SOURCES
    parser.cpp
    latex_nivalis_conv.cpp
    env.cpp
    expr.cpp
    util.cpp
    opcodes.cpp
    eval_expr.cpp
    shell.cpp
    optimize_expr.cpp
    color.cpp
    point.cpp
    plotter/gui.cpp
    plotter/render.cpp
    plotter/imgui_adaptor.cpp
)
list(TRANSFORM SOURCES PREPEND ${SRC_DIR}/)
set(
    IMGUI_SOURCES
    imgui.cpp
    imgui_draw.cpp
    imgui_widgets.cpp
    imgui_impl_opengl3.cpp
    imgui_impl_glfw.cpp
    imgui_stdlib.cpp
)
list(TRANSFORM IMGUI_SOURCES PREPEND ${IMGUI_DIR}/)
set ( SOURCES ${SOURCES} ${IMGUI_SOURCES} )
set( PROJ_DEPENDENCIES )

include_directories(
    ${INCLUDE_DIR}
    ${INCLUDE_DIR_3RDPARTY}
    ${PROJECT_BINARY_DIR}/include
    ${X11_INCLUDE_DIR}
    ${Fontconfig_INCLUDE_DIRS}
    ${IMGUI_DIR}
)

set(_EMSCRIPTEN_ "//")

set ( _BOOST_ENABLED_ "//" )
set ( BOOST_ENABLED OFF )

add_definitions( -DNANOVG_GL3_IMPLEMENTATION )

find_package(Threads REQUIRED)

set ( _READLINE_ENABLED_ "//" )
if (NOT WIN32)
    # Find Readline
    find_package( Readline )
    if ( READLINE_FOUND )
        set ( _READLINE_ENABLED_ "" )
        set( PROJ_DEPENDENCIES ${PROJ_DEPENDENCIES} ${READLINE_LIBRARIES} )
        include_directories(${READLINE_INCLUDE_DIRS})
    endif()
endif()

set (SOURCES ${SOURCES} "../${GLEW_DIR}/glew.c")
# Finding OpenGL/GLFW
set ( OPENGL_IMGUI_ENABLED OFF )
set ( WILL_USE_SYSTEM_GLFW ${USE_SYSTEM_GLFW} )
add_definitions(-DGLEW_STATIC)

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
    SET(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "GLFW example" FORCE)
    SET(GLFW_BUILD_TESTS OFF CACHE BOOL "GLFW tests" FORCE)
    SET(GLFW_BUILD_DOCS OFF CACHE BOOL "GLFW docs" FORCE)
    SET(GLFW_INSTALL OFF CACHE BOOL "GLFW install" FORCE)
    add_subdirectory( "${GLFW_DIR}" )
    set( PROJ_DEPENDENCIES ${PROJ_DEPENDENCIES} glfw)
    include_directories( "${GLFW_DIR}/include" )
else()
    message ( STATUS "Using system glfw3" )
endif ()

find_package(OpenGL REQUIRED)

set ( OPENGL_IMGUI_ENABLED ON )
set( PROJ_DEPENDENCIES
    OpenGL::GL
    ${PROJ_DEPENDENCIES}
    ${GLFW_STATIC_LIBRARIES}
)

include_directories(${GLEW_DIR})

message ( STATUS "Using OpenGL with ImGui as plotter backend" )

add_library( ${LIB_PROJ_NAME} STATIC ${HEADERS} ${SOURCES} )
set_target_properties(${LIB_PROJ_NAME} PROPERTIES OUTPUT_NAME
    ${SHELL_PROJ_NAME} )

foreach(targ ${PROJ_EXECUTABLES})
    add_executable( ${targ} "${SRC_DIR}/main_${targ}.cpp" )
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
        set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g" )
    endif ()
endforeach()

install(TARGETS ${PROJ_EXECUTABLES} DESTINATION bin)

if ( BOOST_ENABLED )
    include_directories( ${Boost_INCLUDE_DIRS} )
    # ensure boost doesn't throw exception when evaluating out of domain
    add_definitions( -DBOOST_MATH_DOMAIN_ERROR_POLICY=ignore_error
        -DBOOST_MATH_POLE_ERROR_POLICY=ignore_error -DBOOST_MATH_OVERFLOW_ERROR_POLICY=ignore_error
        -DBOOST_MATH_UNDERFLOW_ERROR_POLICY=ignore_error -DBOOST_MATH_OVERFLOW_ERROR_POLICY=ignore_error
        -DBOOST_MATH_ROUNDING_ERROR_POLICY=ignore_error
        -DBOOST_MATH_EVALUATION_ERROR_POLICY=ignore_error
        )
    if ( MSVC )
        add_definitions(-D_HAS_AUTO_PTR_ETC=1)
    endif ()
endif ()
if (WIN32)
    add_definitions(-DNOMINMAX)
endif()
# ensure M_PI, etc available
add_definitions( -D_USE_MATH_DEFINES )

# create version.hpp
configure_file( version.hpp.in "${PROJECT_BINARY_DIR}/include/version.hpp" )

# Create source group for headers
source_group( "Header Files" FILES ${HEADERS} )
