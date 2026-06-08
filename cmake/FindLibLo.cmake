find_path(LibLo_INCLUDE_DIR lo/lo.h
    PATHS /usr/local/include /usr/include /opt/homebrew/include
)
find_library(LibLo_LIBRARY
    NAMES lo
    PATHS /usr/local/lib /usr/lib /opt/homebrew/lib
)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibLo DEFAULT_MSG LibLo_LIBRARY LibLo_INCLUDE_DIR)
if(LibLo_FOUND)
    set(LibLo_LIBRARIES ${LibLo_LIBRARY})
    set(LibLo_INCLUDE_DIRS ${LibLo_INCLUDE_DIR})
endif()
mark_as_advanced(LibLo_INCLUDE_DIR LibLo_LIBRARY)
