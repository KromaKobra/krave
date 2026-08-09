qt_add_executable(musicvisualizer
    main.cpp
    AudioEngine.h      AudioEngine.cpp
    ExportController.h ExportController.cpp
    Layer.h            Layer.cpp
    LayerModel.h       LayerModel.cpp
    Project.h          Project.cpp
    SpectrumEngine.h   SpectrumEngine.cpp
)

set(app_qml_files
    qml/Main.qml
    qml/VisualizerCanvas.qml
    qml/layers/LayerFrame.qml
    qml/layers/BarLayerView.qml
    qml/layers/BackgroundLayerView.qml
    qml/layers/TextLayerView.qml
    qml/panels/TransportBar.qml
    qml/panels/TimelineRuler.qml
    qml/panels/LayerListPanel.qml
    qml/panels/PropertyPanel.qml
    qml/panels/ExportDialog.qml
    qml/controls/LabeledSlider.qml
    qml/controls/EnumCombo.qml
    qml/controls/ColorField.qml
)

# The QML sources live under qml/, but the module root must be that directory --
# otherwise Main.qml would register as MusicVisualizer.qml/Main. Aliasing each
# file strips the prefix so `import "panels"` and loadFromModule("...", "Main")
# both resolve.
foreach(qml_file IN LISTS app_qml_files)
    string(REGEX REPLACE "^qml/" "" qml_alias "${qml_file}")
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_alias})
endforeach()

qt_add_qml_module(musicvisualizer
    URI MusicVisualizer
    VERSION 1.0
    QML_FILES ${app_qml_files}
)

target_link_libraries(musicvisualizer PRIVATE
    mvcore
    Qt6::Core
    Qt6::Gui
    # QRhi lives behind the private Gui headers. The export path needs it because
    # QQuickWindow::grabWindow() returns null for a QQuickRenderControl window,
    # so frames must be read back from an explicit texture render target.
    Qt6::GuiPrivate
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
)

target_compile_definitions(musicvisualizer PRIVATE
    MV_VERSION="${PROJECT_VERSION}"
)

set_target_properties(musicvisualizer PROPERTIES
    WIN32_EXECUTABLE FALSE   # keep a console attached for diagnostics
    MACOSX_BUNDLE FALSE
)
