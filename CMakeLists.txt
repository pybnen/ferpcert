cmake_minimum_required(VERSION 3.11)
project(ferpcheck)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-parentheses -O3")

add_custom_target(build_libglucose    
    COMMAND make libr
    COMMAND mv lib_release.a libglucose.a
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/glucose-syrup/simp/ 
)

set(COMMON_FILES
    Clause.cpp
    FerpManager.cpp
    FerpReader.cpp
    Formula.cpp
    QbfReader.cpp
    Quant.cpp
    Reader.cpp    
)

set(FERPCHECK_FILES
    ferpcheck-main.cpp
    ipasir/ipasir-glucose4.cc
)

set(FERPCERT_FILES
    ferpcert-main.cpp
    aiger.c
    aiger.h
)

add_executable(ferpcheck ${COMMON_FILES} ${FERPCHECK_FILES})
add_executable(ferpcert ${COMMON_FILES} ${FERPCERT_FILES})

add_dependencies(ferpcheck build_libglucose)

find_package( ZLIB REQUIRED )
if ( ZLIB_FOUND )
    include_directories( ${ZLIB_INCLUDE_DIRS} )
    # target_link_libraries( ferpcheck ${ZLIB_LIBRARIES}, -Wl, -Bstatic -l:libglucose.a -lz -Wl,-Bdynamic )
    target_link_libraries( ferpcert ${ZLIB_LIBRARIES} )
endif( ZLIB_FOUND )

target_include_directories(ferpcheck PRIVATE ${CMAKE_SOURCE_DIR}/glucose-syrup/)
target_compile_options(ferpcheck PRIVATE -O3 -DNDEBUG -Wall -Wno-parentheses -std=c++11)
target_link_directories(ferpcheck PRIVATE ${CMAKE_SOURCE_DIR}/glucose-syrup/simp)
target_link_libraries(ferpcheck PRIVATE -Wl,-Bstatic -l:libglucose.a -lz -Wl,-Bdynamic)

set_target_properties(ferpcheck PROPERTIES COMPILE_DEFINITIONS "FERP_CHECK")
set_target_properties(ferpcert PROPERTIES COMPILE_DEFINITIONS "FERP_CERT")