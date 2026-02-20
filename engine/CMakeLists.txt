cmake_minimum_required(VERSION 3.20)
project(bb_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Library
add_library(bb_engine SHARED
    src/position.cpp
    src/game_state.cpp
    src/dice.cpp
    src/helpers.cpp
    src/injury.cpp
    src/ball_handler.cpp
    src/move_handler.cpp
    src/block_handler.cpp
    src/foul_handler.cpp
    src/turn_handler.cpp
    src/pathfinder.cpp
    src/rules_engine.cpp
    src/action_resolver.cpp
    src/pass_handler.cpp
    src/big_guy_handler.cpp
    src/kickoff_handler.cpp
    src/roster.cpp
    src/game_simulator.cpp
    src/feature_extractor.cpp
    src/value_function.cpp
    src/mcts.cpp
    src/policies.cpp
    src/action_features.cpp
    src/policy_network.cpp
    src/ttm_handler.cpp
    src/bomb_handler.cpp
    src/gaze_handler.cpp
    src/ball_and_chain_handler.cpp
    src/macro_actions.cpp
    src/macro_mcts.cpp
)
target_include_directories(bb_engine PUBLIC include third_party)

# Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(bb_tests
    tests/test_enums.cpp
    tests/test_position.cpp
    tests/test_player.cpp
    tests/test_game_state.cpp
    tests/test_dice.cpp
    tests/test_helpers.cpp
    tests/test_injury.cpp
    tests/test_ball_handler.cpp
    tests/test_move_handler.cpp
    tests/test_block_handler.cpp
    tests/test_foul_handler.cpp
    tests/test_rules_engine.cpp
    tests/test_action_resolver.cpp
    tests/test_pass_handler.cpp
    tests/test_big_guy_handler.cpp
    tests/test_kickoff_handler.cpp
    tests/test_feature_extractor.cpp
    tests/test_value_function.cpp
    tests/test_mcts.cpp
    tests/test_game_simulator.cpp
    tests/test_ttm_handler.cpp
    tests/test_bomb_handler.cpp
    tests/test_gaze_handler.cpp
    tests/test_ball_and_chain_handler.cpp
    tests/test_action_features.cpp
    tests/test_policy_network.cpp
    tests/test_macro_actions.cpp
    tests/test_macro_mcts.cpp
)
target_link_libraries(bb_tests PRIVATE bb_engine GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(bb_tests)

# MCTS CLI
add_executable(mcts_cli cli/mcts_cli.cpp)
target_link_libraries(mcts_cli PRIVATE bb_engine)

# Python bindings (pybind11)
find_package(pybind11 QUIET)
if(pybind11_FOUND)
    pybind11_add_module(bb_engine_py python/bb_module.cpp)
    target_link_libraries(bb_engine_py PRIVATE bb_engine)
    set_target_properties(bb_engine_py PROPERTIES OUTPUT_NAME bb_engine)
    message(STATUS "pybind11 found - building Python module")
else()
    message(STATUS "pybind11 not found - skipping Python module")
endif()
