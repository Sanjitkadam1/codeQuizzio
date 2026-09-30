add_executable(cq_tests
    AnswerCheckerTests.cpp
    ScoringTests.cpp
    QuestionBankTests.cpp
    SelectorTests.cpp
    SessionTests.cpp)

target_link_libraries(cq_tests PRIVATE cq_core GTest::gtest_main)
target_compile_definitions(cq_tests PRIVATE
    CQ_QUESTIONS_DIR="${CMAKE_SOURCE_DIR}/questions")

include(GoogleTest)
gtest_discover_tests(cq_tests)
