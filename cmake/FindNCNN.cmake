find_path(NCNN_INCLUDE_DIR ncnn/net.h
    PATHS /usr/local/include /usr/include /opt/homebrew/include
)
find_library(NCNN_LIBRARY
    NAMES ncnn
    PATHS /usr/local/lib /usr/lib /opt/homebrew/lib
)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NCNN DEFAULT_MSG NCNN_LIBRARY NCNN_INCLUDE_DIR)
if(NCNN_FOUND)
    if(NOT TARGET ncnn)
        add_library(ncnn UNKNOWN IMPORTED)
        set_target_properties(ncnn PROPERTIES
            IMPORTED_LOCATION "${NCNN_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${NCNN_INCLUDE_DIR}"
        )
    endif()
endif()
mark_as_advanced(NCNN_INCLUDE_DIR NCNN_LIBRARY)
