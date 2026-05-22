# Декларация тестов.
add_executable(hydra_core_tests
    core_tests/metric_aggregator_test.cpp
    core_tests/protocol_parser_test.cpp
)
target_link_libraries(hydra_core_tests PRIVATE
    hydra_core
    GTest::gtest_main
)

add_test(NAME hydra_core_tests COMMAND hydra_core_tests)
