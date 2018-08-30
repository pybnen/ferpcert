cmake_minimum_required(VERSION 3.11)
project(ferpcheck)

set(CMAKE_CXX_STANDARD 11)

set(COMMON_FILES
    common.h
    parseutils.h
    Reader.cpp
    Reader.h
    FerpManager.cpp
    FerpManager.h
    FerpReader.cpp
    FerpReader.h

    Formula.cpp
    Formula.h
    Quant.cpp
    Quant.h
    Clause.cpp
    Clause.h
    QbfReader.cpp
    QbfReader.h
   )

set(FERPCHECK_FILES
    ferpcheck-main.cpp
   )

set(FERPCERT_FILES
    ferpcert-main.cpp
    aiger.c
    aiger.h
        )

add_executable(ferpcheck ${COMMON_FILES} ${FERPCHECK_FILES})
add_executable(ferpcert ${COMMON_FILES} ${FERPCERT_FILES})

find_package( ZLIB REQUIRED )
if ( ZLIB_FOUND )
    include_directories( ${ZLIB_INCLUDE_DIRS} )
    target_link_libraries( ferpcheck ${ZLIB_LIBRARIES} )
    target_link_libraries( ferpcert ${ZLIB_LIBRARIES} )
endif( ZLIB_FOUND )

set_target_properties(ferpcheck PROPERTIES COMPILE_DEFINITIONS "FERP_CHECK")
set_target_properties(ferpcert PROPERTIES COMPILE_DEFINITIONS "FERP_CERT")