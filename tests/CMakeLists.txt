add_executable(vix_engine_model_tests
  EngineModelTests.cpp
)
target_link_libraries(vix_engine_model_tests PRIVATE vix::engine)
add_test(NAME vix_engine_model_tests COMMAND vix_engine_model_tests)

add_executable(vix_engine_parser_tests
  ParserTests.cpp
)
target_link_libraries(vix_engine_parser_tests PRIVATE vix::engine)
add_test(NAME vix_engine_parser_tests COMMAND vix_engine_parser_tests)

add_executable(vix_engine_ninja_tests
  NinjaTests.cpp
)
target_link_libraries(vix_engine_ninja_tests PRIVATE vix::engine)
add_test(NAME vix_engine_ninja_tests COMMAND vix_engine_ninja_tests)

add_executable(vix_engine_graph_tests
  GraphTests.cpp
)
target_link_libraries(vix_engine_graph_tests PRIVATE vix::engine)
add_test(NAME vix_engine_graph_tests COMMAND vix_engine_graph_tests)
