if(TARGET jsoncpp_lib)
    set(JSONCPP_INCLUDE_DIRS "${jsoncpp_SOURCE_DIR}/include")
    set(JSONCPP_LIBRARIES jsoncpp_lib)

    if(NOT TARGET Jsoncpp_lib)
        add_library(Jsoncpp_lib INTERFACE IMPORTED)
        target_link_libraries(Jsoncpp_lib INTERFACE jsoncpp_lib)
        target_include_directories(Jsoncpp_lib INTERFACE "${jsoncpp_SOURCE_DIR}/include")
    endif()

    set(JSONCPP_FOUND TRUE)
    return()
endif()

find_path(JSONCPP_INCLUDE_DIRS
    NAMES json/json.h
)

find_library(JSONCPP_LIBRARIES
    NAMES jsoncpp_lib jsoncpp
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Jsoncpp
    DEFAULT_MSG
    JSONCPP_INCLUDE_DIRS
    JSONCPP_LIBRARIES
)

if(JSONCPP_FOUND AND NOT TARGET Jsoncpp_lib)
    add_library(Jsoncpp_lib INTERFACE IMPORTED)
    set_target_properties(Jsoncpp_lib PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${JSONCPP_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${JSONCPP_LIBRARIES}"
    )
endif()