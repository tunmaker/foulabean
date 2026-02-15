#src/main/CMakeLists.txt

set(TARGET_NAME appdigitwin)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.8)

set(MAIN_QML
    qml/Main.qml
)

qt_add_executable(${TARGET_NAME}
    src/main.cpp
    src/BridgeTypes.h
    src/RenodeWorker.h src/RenodeWorker.cpp
    src/GpioModel.h src/GpioModel.cpp
    src/AdcModel.h src/AdcModel.cpp
    src/SimulationController.h src/SimulationController.cpp
)

qt_add_qml_module(${TARGET_NAME}
    URI digitwin
    VERSION 1.0
    QML_FILES
        ${MAIN_QML}
    SOURCES
        src/SimulationController.h src/SimulationController.cpp
        src/GpioModel.h src/GpioModel.cpp
        src/AdcModel.h src/AdcModel.cpp
)

set_target_properties(${TARGET_NAME} PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_include_directories(${TARGET_NAME}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${TARGET_NAME}
    PRIVATE Qt6::Quick
    PRIVATE renodeAPI::renodeAPI
)

include(GNUInstallDirs)
install(TARGETS ${TARGET_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
