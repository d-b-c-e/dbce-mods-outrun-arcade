cmake_minimum_required(VERSION 3.20)
project(CabinetForceTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
enable_testing()
add_executable(cabinet_force_test cabinet_force_test.cpp)
add_test(NAME cabinet_force COMMAND cabinet_force_test)
# STD-033 rig-profile controls: identity resolve, pedal rest, POV (src/main/frontend/profile_device.hpp).
add_executable(profile_device_test profile_device_test.cpp)
target_include_directories(profile_device_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src/main")
add_test(NAME profile_device COMMAND profile_device_test)
# STD-033 section 6 dev-only test injection for the profile wheel (src/main/sdl2/test_inject.cpp).
add_executable(test_inject_test test_inject_test.cpp ../src/main/sdl2/test_inject.cpp)
target_include_directories(test_inject_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src/main")
add_test(NAME test_inject COMMAND test_inject_test)
# The force latch is process-wide: each order is its own process.
add_test(NAME test_inject_request_first COMMAND test_inject_test --request-first)
add_test(NAME test_inject_force_first COMMAND test_inject_test --force-first)
if(WIN32)
    include(../cmake/VerifySessionWriter.cmake)
    # The captured Windows contract includes explicit neutral sends; Linux's
    # original transport intentionally has a different neutral call boundary.
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/cabinet_producer.inc"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_cabinet_producer.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/../src/main/engine/ooutputs.cpp"
            "${CMAKE_CURRENT_BINARY_DIR}/cabinet_producer.inc"
        DEPENDS extract_cabinet_producer.py ../src/main/engine/ooutputs.cpp VERBATIM)
    add_executable(cabinet_producer_test cabinet_producer_test.cpp "${CMAKE_CURRENT_BINARY_DIR}/cabinet_producer.inc")
    target_include_directories(cabinet_producer_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../src/main")
    add_test(NAME cabinet_producer COMMAND cabinet_producer_test)
    file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/../src/main/engine/ooutputs.cpp" CABINET_PRODUCER_HASH)
    file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/../src/main/directx/cabinet_force.hpp" CABINET_MAPPER_HASH)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../src/main/engine/ooutputs.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/../src/main/directx/cabinet_force.hpp")
    add_executable(cabinet_replay cabinet_replay.cpp "${CMAKE_CURRENT_BINARY_DIR}/cabinet_producer.inc")
    target_include_directories(cabinet_replay PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../src/main")
    target_compile_definitions(cabinet_replay PRIVATE DBCE_CABINET_PRODUCER_SHA256="${CABINET_PRODUCER_HASH}" DBCE_CABINET_MAPPER_SHA256="${CABINET_MAPPER_HASH}")
    add_executable(cabinet_recording_test cabinet_recording_test.cpp
        ../src/main/directx/cabinet_recording.cpp "${CMAKE_CURRENT_BINARY_DIR}/cabinet_producer.inc")
    target_include_directories(cabinet_recording_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../src/main")
    target_compile_definitions(cabinet_recording_test PRIVATE CABINET_RECORDING_TEST_CLOCK DBCE_CABINET_PRODUCER_SHA256="${CABINET_PRODUCER_HASH}" DBCE_CABINET_MAPPER_SHA256="${CABINET_MAPPER_HASH}")
    foreach(case complete interrupted lost-lease nested write-failure bad-id bad-duration existing haptic-off cabinet-mode unexpected-delivery early-delivery)
        add_test(NAME recording_${case} COMMAND cabinet_recording_test ${case})
    endforeach()
    add_test(NAME recording_strict_replay COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_cabinet_replay.py" "$<TARGET_FILE:cabinet_recording_test>" "$<TARGET_FILE:cabinet_replay>")
    add_test(NAME recording_force_export COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_cabinet_export.py" "$<TARGET_FILE:cabinet_recording_test>" "$<TARGET_FILE:cabinet_replay>")
    add_executable(toolkit_adapter_test toolkit_adapter_test.cpp)
    foreach(test mute capture-mute missing-guid zero-guid background partial-load wrong-version init-failure guard-failure live-fake transient saved-guid invalid-override override catalog retire-choice)
        add_test(NAME adapter_${test} COMMAND toolkit_adapter_test ${test})
    endforeach()
endif()
