cmake_minimum_required(VERSION 3.24)
project(ControlPanel VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)

add_library(controlpanel STATIC
    src/model.cpp src/model.h
    src/actions.cpp src/actions.h
    src/panelview.cpp src/panelview.h
    src/windows.cpp src/windows.h
    src/application.cpp src/application.h
)
target_link_libraries(controlpanel PUBLIC Qt6::Widgets)
target_include_directories(controlpanel PUBLIC src)
target_compile_definitions(controlpanel PUBLIC CP_VERSION="${PROJECT_VERSION}")
target_compile_options(controlpanel PRIVATE -Wall -Wextra -Wpedantic)

foreach(app IN ITEMS Interpreter Creator)
    add_executable(ControlPanel${app} MACOSX_BUNDLE src/main.cpp)
    target_link_libraries(ControlPanel${app} PRIVATE controlpanel)
    if(app STREQUAL "Creator")
        target_compile_definitions(ControlPanel${app} PRIVATE CP_CREATOR)
        set(CP_DOCUMENT_ROLE Editor)
        set(CP_HANDLER_RANK Alternate)
    else()
        set(CP_DOCUMENT_ROLE Viewer)
        set(CP_HANDLER_RANK Owner)
    endif()
    configure_file(resources/Info.plist.in "${app}-Info.plist.in" @ONLY)
    set_target_properties(ControlPanel${app} PROPERTIES
        OUTPUT_NAME "ControlPanel ${app}"
        MACOSX_BUNDLE_BUNDLE_NAME "ControlPanel ${app}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.gnaservices.ControlPanel.${app}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/${app}-Info.plist.in"
    )
    if(APPLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/${app}.icns")
        target_sources(ControlPanel${app} PRIVATE resources/${app}.icns)
        set_source_files_properties(resources/${app}.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
        set_target_properties(ControlPanel${app} PROPERTIES MACOSX_BUNDLE_ICON_FILE "${app}.icns")
    endif()
    install(TARGETS ControlPanel${app} BUNDLE DESTINATION . RUNTIME DESTINATION bin)
endforeach()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(controlpanel_tests tests/test_controlpanel.cpp)
    target_link_libraries(controlpanel_tests PRIVATE controlpanel Qt6::Test)
    add_test(NAME controlpanel COMMAND controlpanel_tests)
    set_tests_properties(controlpanel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
endif()
