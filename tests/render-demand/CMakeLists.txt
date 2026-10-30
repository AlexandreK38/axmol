cmake_minimum_required(VERSION 3.22)
project(render-demand-state-tests LANGUAGES CXX)
enable_testing()
find_package(Threads REQUIRED)
add_executable(render-demand-state-tests
    StateTests.cpp
    ../../core/base/RenderActivity.cpp
    ../../core/base/RenderDemand.cpp
    ../../3rdparty/doctest/doctest.cpp)
target_compile_features(render-demand-state-tests PRIVATE cxx_std_17)
target_include_directories(render-demand-state-tests PRIVATE ../../core ../../3rdparty/doctest)
target_link_libraries(render-demand-state-tests PRIVATE Threads::Threads)
add_test(NAME render-demand-state COMMAND render-demand-state-tests)
