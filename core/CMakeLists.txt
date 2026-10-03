add_library(windows_live_ime_core STATIC
    src/live_conversion_coordinator.cpp
    src/unicode.cpp
)

target_include_directories(windows_live_ime_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(windows_live_ime_core PUBLIC cxx_std_23)
windows_live_ime_enable_warnings(windows_live_ime_core)

if(BUILD_TESTING)
    add_executable(core_tests tests/core_tests.cpp)
    target_link_libraries(core_tests PRIVATE windows_live_ime_core)
    windows_live_ime_enable_warnings(core_tests)
    add_test(NAME core_tests COMMAND core_tests)
endif()
