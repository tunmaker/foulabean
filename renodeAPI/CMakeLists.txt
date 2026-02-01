#src/submodule/CMaketLists.txt
set(MODULE_NAME renodeAPI)
set(MODULE_ALIAS ${MODULE_NAME}::${MODULE_NAME})  # exported alias
set(MODULE_PUBLIC_INCLUDE include)   # relative include dir

find_package(cppzmq REQUIRED)

set(MODULE_PUBLIC_HEADERS
    include/renodeInterface.h
    include/renodeMachine.h
    include/defs.h
)

set(MODULE_SOURCES
    src/renodeInterface.cpp
    src/renodeMachine.cpp
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

target_link_libraries(${MODULE_NAME}
  PUBLIC cppzmq
)

target_compile_features(${MODULE_NAME} PUBLIC cxx_std_20)

# create a consistent exported alias for consumers
if (NOT TARGET ${MODULE_ALIAS})
    add_library(${MODULE_ALIAS} ALIAS ${MODULE_NAME})
endif()

