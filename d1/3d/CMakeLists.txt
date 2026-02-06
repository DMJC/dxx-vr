add_library(3d STATIC
    clipper.c
    draw.c
    globvars.c
    instance.c
    interp.c
    matrix.c
    points.c
    rod.c
    setup.c
    )

include_directories(../include ../arch/include ../main)

if(APPLE)
    # Needed to define the globals in function-less globvars.c
    target_compile_options(3d PRIVATE -fno-common)
endif()

find_package(SDL2 REQUIRED)
find_package(PhysFS)

if(TARGET SDL2::SDL2)
    target_link_libraries(3d PUBLIC SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
    target_link_libraries(3d PUBLIC SDL2::SDL2-static)
else()
    target_include_directories(3d PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(3d PUBLIC ${SDL2_LIBRARIES})
endif()
target_include_directories(3d PUBLIC ${PHYSFS_INCLUDE_DIR})
