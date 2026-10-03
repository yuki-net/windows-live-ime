add_library(ime_pipe_protocol STATIC
    src/pipe_protocol.cpp
)
target_include_directories(ime_pipe_protocol
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/core/include
)
target_link_libraries(ime_pipe_protocol PUBLIC windows_live_ime_core)
target_compile_features(ime_pipe_protocol PUBLIC cxx_std_23)
windows_live_ime_enable_warnings(ime_pipe_protocol)

add_library(ime_named_pipe_client STATIC
    src/named_pipe_client.cpp
)
target_include_directories(ime_named_pipe_client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ime_named_pipe_client PUBLIC ime_pipe_protocol PRIVATE kernel32)
target_compile_definitions(ime_named_pipe_client PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
windows_live_ime_enable_warnings(ime_named_pipe_client)

add_executable(ime_ipc_ping tools/ipc_ping_main.cpp)
target_link_libraries(ime_ipc_ping PRIVATE ime_named_pipe_client)
target_compile_definitions(ime_ipc_ping PRIVATE UNICODE _UNICODE)
windows_live_ime_enable_warnings(ime_ipc_ping)

if(BUILD_TESTING)
    add_executable(ime_protocol_tests tests/pipe_protocol_tests.cpp)
    target_link_libraries(ime_protocol_tests PRIVATE ime_pipe_protocol)
    windows_live_ime_enable_warnings(ime_protocol_tests)
    add_test(NAME ime_protocol_tests COMMAND ime_protocol_tests)
endif()

if(WINDOWS_LIVE_IME_BUILD_TSF)
    add_library(windows_live_ime SHARED
        src/class_factory.cpp
        src/dllmain.cpp
        src/registration.cpp
        src/text_service.cpp
        windows_live_ime.def
    )
    target_include_directories(windows_live_ime
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/core/include
    )
    target_link_libraries(windows_live_ime
        PRIVATE
            windows_live_ime_core
            ime_named_pipe_client
            advapi32
            ole32
            uuid
    )
    target_compile_definitions(windows_live_ime PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    set_target_properties(windows_live_ime PROPERTIES
        OUTPUT_NAME "windows-live-ime"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/artifacts/$<CONFIG>"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/artifacts/$<CONFIG>"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/artifacts/$<CONFIG>"
    )
    windows_live_ime_enable_warnings(windows_live_ime)
endif()
