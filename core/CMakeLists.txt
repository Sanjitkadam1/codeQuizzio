add_library(cq_core STATIC
    QuestionBank.cpp
    AnswerChecker.cpp
    Scoring.cpp
    AdaptiveSelector.cpp
    Progress.cpp
    ProgressStore.cpp
    Session.cpp)

target_include_directories(cq_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cq_core PUBLIC nlohmann_json::nlohmann_json)

if(MSVC)
    target_compile_options(cq_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(cq_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
