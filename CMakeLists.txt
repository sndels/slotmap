cmake_minimum_required(VERSION 3.20)
project(slotmap)

add_library(slotmap INTERFACE ${CMAKE_CURRENT_LIST_DIR}/slotmap.hpp)

target_include_directories(slotmap
    INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

add_executable(slotmap_test ${CMAKE_CURRENT_LIST_DIR}/test.cpp)
target_compile_features(slotmap_test
    PUBLIC
    cxx_std_20
)

if(MSVC)
    # From cppbestpractices
    target_compile_options(slotmap_test
        PRIVATE
        /permissive-
        /W4
        /w14242
        /w14254
        /w14263
        /w14265
        /w14287
        /we4289
        /w14296
        /w14311
        /w14545
        /w14546
        /w14547
        /w14549
        /w14555
        /w14619
        /w14640
        /w14826
        /w14905
        /w14906
        /w14928
    )
else()
    target_compile_options(slotmap_test
        PRIVATE
        -pedantic
        -Wall
        -Wextra
        -Wunused
    )
endif() # NOT MSVC

target_link_libraries(slotmap_test PRIVATE slotmap)
