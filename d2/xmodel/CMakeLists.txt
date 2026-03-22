add_library(xmodel STATIC
aseread.cpp
submodel.cpp
tga.cpp
xmodel.cpp
	)

find_package(SDL3 REQUIRED)
find_package(GLEW REQUIRED)

include_directories(../include ../arch/include ../main)
target_include_directories(xmodel PUBLIC ${SDL3_INCLUDE_DIRS} ${GLEW_INCLUDE_DIR})
