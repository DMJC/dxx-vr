add_library(mem STATIC
    mem.c
    )

include_directories(../include ../arch/include ../main)

find_package(SDL3 REQUIRED)
find_package(PhysFS)
target_include_directories(mem PUBLIC ${SDL3_INCLUDE_DIRS} ${PHYSFS_INCLUDE_DIR})
