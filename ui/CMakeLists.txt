qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

# On Windows, windeployqt copies the Qt DLLs/plugins next to an exe so it runs
# by double-click (and tests run without Qt on PATH).
if(WIN32)
    find_program(CQ_WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
endif()
function(cq_deploy_qt target)
    if(CQ_WINDEPLOYQT)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CQ_WINDEPLOYQT} --no-translations ${ARGN} $<TARGET_FILE:${target}>)
    endif()
endfunction()

# ---- The desktop app ----
qt_add_executable(cq_app)

qt_add_qml_module(cq_app
    URI CodeQuizzio
    VERSION 1.0
    SOURCES
        QuizController.h
        QuizController.cpp
        main.cpp
    QML_FILES
        qml/Main.qml)

target_link_libraries(cq_app PRIVATE cq_core Qt6::Quick Qt6::QuickControls2)
target_compile_definitions(cq_app PRIVATE
    CQ_DEFAULT_QUESTIONS_DIR="${CMAKE_SOURCE_DIR}/questions")
set_target_properties(cq_app PROPERTIES WIN32_EXECUTABLE ON)

# Ship the question packs next to the executable so the app is self-contained.
add_custom_command(TARGET cq_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/questions $<TARGET_FILE_DIR:cq_app>/questions)
cq_deploy_qt(cq_app --qmldir ${CMAKE_CURRENT_SOURCE_DIR}/qml)

# ---- Controller tests ----
# The controller is compiled directly into the test binary (an executable
# target can't be linked against).
if(CQ_BUILD_TESTS)
    qt_add_executable(cq_ui_tests
        tests/QuizControllerTests.cpp
        QuizController.h
        QuizController.cpp)
    target_include_directories(cq_ui_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(cq_ui_tests PRIVATE cq_core Qt6::Core Qt6::Test GTest::gtest_main)
    target_compile_definitions(cq_ui_tests PRIVATE
        CQ_DEFAULT_QUESTIONS_DIR="${CMAKE_SOURCE_DIR}/questions"
        CQ_TEST_QUESTIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    cq_deploy_qt(cq_ui_tests)
    add_test(NAME cq_ui_tests COMMAND cq_ui_tests)
endif()
