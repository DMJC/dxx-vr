add_library(maths STATIC
    fixc.c
    rand.c
    tables.c
    vecmat.c
    )

include_directories(../include ../arch/include ../main)

find_package(SDL3 REQUIRED)
target_include_directories(maths PUBLIC ${SDL3_INCLUDE_DIRS})
