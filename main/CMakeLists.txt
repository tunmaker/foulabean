#src/main/CMAkeLists.txt

set(TARGET_NAME appdigitwin)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.8)

set(MAIN_PUBLIC_HEADERS
    #include/main/app.hpp
)
set(MAIN_SOURCES
    src/main.cpp
    #rc/app.cpp
)

set(MAIN_QML
    qml/Main.qml
)

qt_add_executable(${TARGET_NAME}
    ${MAIN_SOURCES}
    ${MAIN_PUBLIC_HEADERS}
)

qt_add_qml_module(${TARGET_NAME}
    URI digitwin
    VERSION 1.0
    QML_FILES
        ${MAIN_QML}
)

set_target_properties(${TARGET_NAME} PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(${TARGET_NAME}
    PRIVATE Qt6::Quick
    #PRIVATE renodeAPI::renodeAPI
)

#target_sources(${TARGET_NAME}
#    PRIVATE ${MAIN_SOURCES}
#    PUBLIC  ${MAIN_PUBLIC_HEADERS}
#)
#
#target_include_directories(${TARGET_NAME} PRIVATE
#    ${CMAKE_CURRENT_SOURCE_DIR}/include
#)

include(GNUInstallDirs)
install(TARGETS ${TARGET_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
