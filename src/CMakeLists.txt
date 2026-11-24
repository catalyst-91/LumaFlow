# ---------------------------------------------------------------- lumacore
# Engine library: everything that does not need a widget toolkit.
set(LF_CORE_DIRS core protocols fixtures pixelmapping spatial console timeline
    storage audio ai sync remote plugins recovery media effects nodes layers)

set(LF_CORE_SOURCES "")
foreach(dir ${LF_CORE_DIRS})
    file(GLOB _srcs CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/*.h)
    list(APPEND LF_CORE_SOURCES ${_srcs})
endforeach()

add_library(lumacore STATIC ${LF_CORE_SOURCES} ${CMAKE_SOURCE_DIR}/resources/resources.qrc)
target_include_directories(lumacore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/sdk/include)
target_link_libraries(lumacore PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::Xml Qt6::Concurrent
                                      ZLIB::ZLIB SQLite::SQLite3)
target_compile_definitions(lumacore PUBLIC LUMAFLOW_VERSION="${PROJECT_VERSION}")
if(LF_HAVE_FFMPEG)
    target_link_libraries(lumacore PUBLIC PkgConfig::LIBAV)
    target_compile_definitions(lumacore PUBLIC LF_HAVE_FFMPEG=1)
endif()
if(LF_HAVE_ALSA)
    target_link_libraries(lumacore PUBLIC PkgConfig::ALSA)
    target_compile_definitions(lumacore PUBLIC LF_HAVE_ALSA=1)
endif()
if(WIN32)
    target_link_libraries(lumacore PUBLIC winmm)
endif()
set_target_properties(lumacore PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------- lumarender
# GPU renderer: OpenGL 3.3 core / GLES 3 compatible via Qt's GL abstraction.
file(GLOB LF_RENDER_SOURCES CONFIGURE_DEPENDS renderer/*.cpp renderer/*.h output/*.cpp output/*.h)
add_library(lumarender STATIC ${LF_RENDER_SOURCES})
target_link_libraries(lumarender PUBLIC lumacore Qt6::OpenGL)
set_target_properties(lumarender PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------- app engine
file(GLOB LF_APPCORE_SOURCES CONFIGURE_DEPENDS app/Engine*.cpp app/Engine*.h app/Show*.cpp app/Show*.h)
add_library(lumaengine STATIC ${LF_APPCORE_SOURCES})
target_link_libraries(lumaengine PUBLIC lumarender)

# ---------------------------------------------------------------- CLI
add_executable(lumaflow-cli app/cli_main.cpp)
target_link_libraries(lumaflow-cli PRIVATE lumaengine)
install(TARGETS lumaflow-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# ---------------------------------------------------------------- GUI
if(LUMAFLOW_BUILD_GUI)
    file(GLOB LF_UI_SOURCES CONFIGURE_DEPENDS ui/*.cpp ui/*.h)
    add_executable(lumaflow WIN32 MACOSX_BUNDLE app/gui_main.cpp ${LF_UI_SOURCES})
    target_link_libraries(lumaflow PRIVATE lumaengine Qt6::Widgets Qt6::OpenGLWidgets)
    set_target_properties(lumaflow PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "LUMAFLOW"
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.lumaflow.app"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
    install(TARGETS lumaflow
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
