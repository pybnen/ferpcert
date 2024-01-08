cmake_minimum_required(VERSION 3.11)
project(ferpcheck)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-parentheses -O3")

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

add_definitions(-DUSE_GLUCOSE)
list(APPEND COMMON_FILES "ipasir/ipasir-glucose4.cc")

add_executable(ferpcheck ${COMMON_FILES} ${FERPCHECK_FILES})
add_executable(ferpcert ${COMMON_FILES} ${FERPCERT_FILES})

target_include_directories(ferpcheck PRIVATE "./glucose-syrup/")
target_link_directories(ferpcheck PRIVATE "./glucose-syrup/simp")

set(LIBS "-lz")
list(APPEND LIBS "glucose")

find_package( ZLIB REQUIRED )
if ( ZLIB_FOUND )
    include_directories( ${ZLIB_INCLUDE_DIRS} )
    list(APPEND LIBS ${ZLIB_LIBRARIES})
endif( ZLIB_FOUND )

target_link_libraries(ferpcheck PRIVATE ${LIBS})
target_link_libraries(ferpcert PRIVATE ${LIBS})

set_target_properties(ferpcheck PROPERTIES COMPILE_DEFINITIONS "FERP_CHECK")
set_target_properties(ferpcert PROPERTIES COMPILE_DEFINITIONS "FERP_CERT")