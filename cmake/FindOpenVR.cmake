# Find OpenVR
#
# OPENVR_INCLUDE_DIR
# OPENVR_LIBRARY
# OPENVR_FOUND

find_path(OPENVR_INCLUDE_DIR openvr.h
    PATH_SUFFIXES openvr
    PATHS
    /usr/include
    /usr/local/include
    /opt/local/include
    /sw/include
    /opt/homebrew/include
)

find_library(OPENVR_LIBRARY
    NAMES openvr_api openvr
    PATHS
    /usr/lib
    /usr/local/lib
    /opt/local/lib
    /sw/lib
    /opt/homebrew/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenVR DEFAULT_MSG OPENVR_LIBRARY OPENVR_INCLUDE_DIR)
