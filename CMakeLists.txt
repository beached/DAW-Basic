cmake_minimum_required( VERSION 2.8.0 )
project( daw_basic_prj )

include( ExternalProject )

set( Boost_USE_STATIC_LIBS OFF )
set( Boost_USE_MULTITHREADED ON )
set( Boost_USE_STATIC_RUNTIME OFF )
find_package( Boost 1.59.0 REQUIRED COMPONENTS system filesystem unit_test_framework date_time )

if( ${CMAKE_CXX_COMPILER_ID} STREQUAL 'MSVC' )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_WIN32_WINNT=0x0601" )
elseif( ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -stdlib=libc++ -g -Weverything -Wno-c++98-compat  -Wfatal-errors -Wno-covered-switch-default -Wno-padded -Wno-exit-time-destructors -Wno-c++98-compat-pedantic -Wno-unused-parameter -Wno-missing-noreturn -Wno-missing-prototypes -Wno-disabled-macro-expansion" )
elseif( ${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -g -O3 -Wall -Wno-deprecated-declarations" )
endif( )

include_directories( SYSTEM ${Boost_INCLUDE_DIRS} )

set( HEADER_FOLDER "include" )
include_directories( ${HEADER_FOLDER} )

set( HEADER_FILES
	${HEADER_FOLDER}/basic_statement.h
	${HEADER_FOLDER}/dawbasic.h
	${HEADER_FOLDER}/mostlyimmutable.h
)

set( SOURCE_FILES
	dawbasic.cpp
	main.cpp
)

add_executable( daw_basic ${SOURCE_FILES} )

target_link_libraries( daw_basic ${CMAKE_DL_LIBS} ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${COMPILER_SPECIFIC_LIBS} )
