qt_add_executable(tst_fft tst_fft.cpp)
target_link_libraries(tst_fft PRIVATE mvcore Qt6::Test)
set_target_properties(tst_fft PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME tst_fft COMMAND tst_fft)

# Project round-trip needs the Layer/Project classes. They come from mvapp, which
# carries its own include directory and Qt dependencies -- so splitting the model
# into more files cannot break this target.
qt_add_executable(tst_project tst_project.cpp)
target_link_libraries(tst_project PRIVATE mvapp Qt6::Test)
set_target_properties(tst_project PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME tst_project COMMAND tst_project)

# It builds a QGuiApplication only to own the QML engine types -- it opens no
# window. Forcing the offscreen platform keeps it runnable over SSH, in CI and
# in the Nix build sandbox, where connecting to a display server would abort.
set_tests_properties(tst_project PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
