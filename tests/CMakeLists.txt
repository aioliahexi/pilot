include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

set(TEST_SOURCES
    test_ring_buffer.cpp
    test_sliding_window.cpp
    test_condition_engine.cpp
    test_vehicle_profile.cpp
)

foreach(test_src ${TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    target_include_directories(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(${test_name} PRIVATE
        pilot_core
        GTest::gtest_main
        GTest::gmock_main
    )
    target_compile_features(${test_name} PRIVATE cxx_std_17)
    gtest_discover_tests(${test_name})
endforeach()
