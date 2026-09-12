# mvapp: the document model -- layer types, the stack and load/save.
#
# Split out of the app target so the tests can link it rather than listing its
# .cpp files one by one. That list drifted every time the model was split into
# another file, and a test target that silently stops compiling half the model
# is a bad way to find out.
#
# No Quick and no Multimedia here: AudioEngine, SpectrumEngine and
# ExportController stay in the executable, so this library builds headless.
add_library(mvapp STATIC
    Layer.h            Layer.cpp
    LayerModel.h       LayerModel.cpp
    Project.h          Project.cpp
)

target_include_directories(mvapp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(mvapp PUBLIC
    mvcore
    Qt6::Core
    Qt6::Gui   # QColor on the layer properties
    Qt6::Qml   # QQmlEngine::setObjectOwnership, Q_INVOKABLE registration
)

set_target_properties(mvapp PROPERTIES AUTOMOC ON)

qt_add_executable(krave
    main.cpp
    AudioEngine.h      AudioEngine.cpp
    ExportController.h ExportController.cpp
    SpectrumEngine.h   SpectrumEngine.cpp
)

set(app_qml_files
    qml/Main.qml
    qml/VisualizerCanvas.qml
    qml/layers/LayerFrame.qml
    qml/layers/BarLayerView.qml
    qml/layers/BackgroundLayerView.qml
    qml/layers/TextLayerView.qml
    qml/layers/ParticleLayerView.qml
    qml/panels/TopBar.qml
    qml/panels/LayerListPanel.qml
    qml/panels/CanvasStage.qml
    qml/panels/PropertyPanel.qml
    qml/panels/TimelinePanel.qml
    qml/panels/ExportDialog.qml
    qml/controls/AppButton.qml
    qml/controls/AppDialog.qml
    qml/controls/AppIcon.qml
    qml/controls/AppScrollBar.qml
    qml/controls/AppSlider.qml
    qml/controls/AppTextField.qml
    qml/controls/AppToolTip.qml
    qml/controls/ColorField.qml
    qml/controls/EnumCombo.qml
    qml/controls/IconButton.qml
    qml/controls/NumberField.qml
    qml/controls/ParamLabel.qml
    qml/controls/ParamSlider.qml
    qml/controls/SectionHeader.qml
    qml/controls/SegmentedControl.qml
    qml/controls/Tag.qml
    qml/controls/ToggleRow.qml
    qml/controls/WaveformStrip.qml
)

# Design tokens, the icon set and the layer-type table. Registered as singletons
# so every QML file reads one instance of each rather than constructing its own.
set(app_qml_singletons
    qml/Theme.qml
    qml/Icons.qml
    qml/LayerTypes.qml
)

list(APPEND app_qml_files ${app_qml_singletons})

set_source_files_properties(${app_qml_singletons}
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# The QML sources live under qml/, but the module root must be that directory --
# otherwise Main.qml would register as krave.qml/Main. Aliasing each
# file strips the prefix so `import "panels"` and loadFromModule("...", "Main")
# both resolve.
foreach(qml_file IN LISTS app_qml_files)
    string(REGEX REPLACE "^qml/" "" qml_alias "${qml_file}")
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_alias})
endforeach()

set_source_files_properties(assets/glowdot.png
    PROPERTIES QT_RESOURCE_ALIAS assets/glowdot.png)

qt_add_qml_module(krave
    URI krave
    VERSION 1.0
    QML_FILES ${app_qml_files}
    RESOURCES
        # ImageParticle's built-in glowdot resource does not load in this Qt
        # build, so the particle sprite ships with the app instead.
        assets/glowdot.png
)

# Blend-mode fragment shaders. Compiled to .qsb and placed alongside the QML in
# the module's resource tree, so LayerFrame can reference them by qrc path.
qt_add_shaders(krave "blend_shaders"
    PREFIX "/qt/qml/krave"
    FILES
        shaders/blend_add.frag
        shaders/blend_screen.frag
    OUTPUTS
        shaders/blend_add.frag.qsb
        shaders/blend_screen.frag.qsb
)

target_link_libraries(krave PRIVATE
    mvapp
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

# QtQuick.Particles is a QML-only import for the particle layer, and
# QtQuick.Shapes is one for the icon set; naming them here ensures the plugins are
# deployed alongside the app.
find_package(Qt6 6.8 QUIET COMPONENTS QuickParticles QuickShapes)

target_compile_definitions(krave PRIVATE
    MV_VERSION="${PROJECT_VERSION}"
)

set_target_properties(krave PROPERTIES
    WIN32_EXECUTABLE FALSE   # keep a console attached for diagnostics
    MACOSX_BUNDLE FALSE
)

include(GNUInstallDirs)
install(TARGETS krave
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE  DESTINATION .
)
