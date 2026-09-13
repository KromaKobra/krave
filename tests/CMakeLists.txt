qt_add_executable(tst_fft tst_fft.cpp)
target_link_libraries(tst_fft PRIVATE mvcore Qt6::Test)
set_target_properties(tst_fft PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME tst_fft COMMAND tst_fft)

# Project round-trip needs the Layer/Project classes. They come from mvapp, which
# carries its own include directory and Qt dependencies -- so splitting the model
# into more files cannot break this target.
qt_add_executable(tst_project tst_project.cpp)
target_link_libraries(tst_project PRIVATE mvapp Qt6::Test)
# The smoke test loads every file in examples/, which is the cheapest guard there
# is on the output of tools/migrate_v2_to_v3.py.
target_compile_definitions(tst_project PRIVATE
    MV_EXAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../examples")
set_target_properties(tst_project PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME tst_project COMMAND tst_project)

# Graph topology: the port table, link validation, the flatten. Separate from
# tst_project so a broken flatten and a broken round-trip report separately.
qt_add_executable(tst_graph tst_graph.cpp)
target_link_libraries(tst_graph PRIVATE mvapp Qt6::Test)
set_target_properties(tst_graph PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME tst_graph COMMAND tst_graph)

# What the editor reopens on launch. Qt Core only, so unlike the two above this
# one needs no platform plugin.
qt_add_executable(tst_session tst_session.cpp)
target_link_libraries(tst_session PRIVATE mvapp Qt6::Test)
set_target_properties(tst_session PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME tst_session COMMAND tst_session)

# Both build a QGuiApplication only to own the QML engine types -- they open no
# window. Forcing the offscreen platform keeps them runnable over SSH, in CI and
# in the Nix build sandbox, where connecting to a display server would abort.
set_tests_properties(tst_project tst_graph PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
