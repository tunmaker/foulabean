#src/submodule/CMaketLists.txt

set(MODULE_NAME renodeAPI)
set(MODULE_ALIAS ${MODULE_NAME}::${MODULE_NAME})  # exported alias
set(MODULE_PUBLIC_INCLUDE include)   # relative include dir

set(MODULE_SOURCES
    src/zmqInterface.cpp
    # add other source files here
)
set(MODULE_PUBLIC_HEADERS
    include/zmqInterface.hpp
    # add public headers here
)

# --- common reuse logic (no changes below) ---
add_library(${MODULE_NAME} STATIC)

target_sources(${MODULE_NAME}
    PRIVATE ${MODULE_SOURCES}
    PUBLIC  ${MODULE_PUBLIC_HEADERS}
)

target_include_directories(${MODULE_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_PUBLIC_INCLUDE}>
        $<INSTALL_INTERFACE:include/${MODULE_NAME}>
)

target_compile_features(${MODULE_NAME} PUBLIC cxx_std_20)

# create a consistent exported alias for consumers
if (NOT TARGET ${MODULE_ALIAS})
    add_library(${MODULE_ALIAS} ALIAS ${MODULE_NAME})
endif()
