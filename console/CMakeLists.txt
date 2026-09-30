add_executable(cq_console main.cpp)
target_link_libraries(cq_console PRIVATE cq_core)
target_compile_definitions(cq_console PRIVATE
    CQ_DEFAULT_QUESTIONS_DIR="${CMAKE_SOURCE_DIR}/questions")
