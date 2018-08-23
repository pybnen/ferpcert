cmake_minimum_required(VERSION 3.11)
project(ferpcheck)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        main.cpp
        common.h
        parseutils.h
        Reader.cpp
        Reader.h
        FerpManager.cpp
        FerpManager.h
        Formula.cpp
        Formula.h
        Quant.cpp
        Quant.h
        Clause.cpp
        Clause.h)

add_executable(ferpcheck ${SOURCE_FILES})

find_package( ZLIB REQUIRED )
if ( ZLIB_FOUND )
    include_directories( ${ZLIB_INCLUDE_DIRS} )
    target_link_libraries( ferpcheck ${ZLIB_LIBRARIES} )
endif( ZLIB_FOUND )