add_library(misc STATIC
    args.c
    error.c
    hash.c
    hmp.c
    ignorecase.c
    physfsx.c
    strio.c
    strutil.c
    )

if(PNG)
    target_sources(misc PRIVATE pngfile.c)
endif()

include_directories(../include ../arch/include ../main)

find_package(SDL2 REQUIRED)
find_package(PhysFS)

if(TARGET SDL2::SDL2)
    target_link_libraries(misc PUBLIC SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
    target_link_libraries(misc PUBLIC SDL2::SDL2-static)
else()
    target_include_directories(misc PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(misc PUBLIC ${SDL2_LIBRARIES})
endif()
target_include_directories(misc PUBLIC ${PHYSFS_INCLUDE_DIR})
