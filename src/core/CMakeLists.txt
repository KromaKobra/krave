# mvcore: audio decoding, frequency analysis and tool discovery.
#
# Deliberately links Qt6::Core only -- no GUI, no QML. That keeps the unit tests
# fast and headless, and lets the export path reuse the analysis code without
# dragging in the scene graph.
add_library(mvcore STATIC
    AudioBuffer.h        AudioBuffer.cpp
    AudioDecoder.h       AudioDecoder.cpp
    Fft.h                Fft.cpp
    FfmpegLocator.h      FfmpegLocator.cpp
    SpectrumAnalyzer.h   SpectrumAnalyzer.cpp
)

target_include_directories(mvcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(mvcore PUBLIC Qt6::Core Qt6::Concurrent)

set_target_properties(mvcore PROPERTIES AUTOMOC ON)
