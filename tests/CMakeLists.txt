qt_add_executable(tst_fft tst_fft.cpp)
target_link_libraries(tst_fft PRIVATE mvcore Qt6::Test)
set_target_properties(tst_fft PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME tst_fft COMMAND tst_fft)

# Project round-trip needs the Layer/Project classes, which live in the app
# target. They are compiled in directly rather than split into another library --
# there are only a few files and it keeps the app's CMake simple.
qt_add_executable(tst_project
    tst_project.cpp
    ../src/app/Layer.cpp
    ../src/app/LayerModel.cpp
    ../src/app/Project.cpp
)
target_include_directories(tst_project PRIVATE ../src/app)
target_link_libraries(tst_project PRIVATE mvcore Qt6::Gui Qt6::Qml Qt6::Test)
set_target_properties(tst_project PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME tst_project COMMAND tst_project)
