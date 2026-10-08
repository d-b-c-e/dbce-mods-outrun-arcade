cmake_minimum_required(VERSION 3.20)
project(CabinetForceTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
enable_testing()
add_executable(cabinet_force_test cabinet_force_test.cpp)
add_test(NAME cabinet_force COMMAND cabinet_force_test)
if(WIN32)
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
    add_executable(toolkit_adapter_test toolkit_adapter_test.cpp)
    foreach(test mute missing-guid zero-guid background partial-load wrong-version init-failure guard-failure live-fake transient saved-guid invalid-override override catalog retire-choice)
        add_test(NAME adapter_${test} COMMAND toolkit_adapter_test ${test})
    endforeach()
endif()
