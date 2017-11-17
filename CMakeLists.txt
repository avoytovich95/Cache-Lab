cmake_minimum_required(VERSION 3.8)
project(cachelab_handout)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        traces/dave.trace
        traces/long.trace
        traces/trans.trace
        traces/yi.trace
        traces/yi2.trace
        cachelab.c
        cachelab.h
        csim-ref
        csim.c
        driver.py
        Makefile
        README
        test-csim
        test-trans.c
        tracegen.c
        trans.c)

add_executable(cachelab_handout ${SOURCE_FILES})