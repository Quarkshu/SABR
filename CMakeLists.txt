cmake_minimum_required(VERSION 3.16)
project(sabr VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- 核心库（无第三方依赖） ---
add_library(sabr_core STATIC
    src/models/contact.cpp
    src/models/contact_plan.cpp
    src/models/bundle.cpp
    src/models/route.cpp
    src/models/node.cpp
)
target_include_directories(sabr_core PUBLIC include)

# --- 算法库 ---
add_library(sabr_algorithms STATIC
    src/algorithms/contact_graph.cpp
    src/algorithms/dijkstra.cpp
    src/algorithms/yen_ksp.cpp
    src/algorithms/phase1.cpp
    src/algorithms/phase2.cpp
    src/algorithms/phase3.cpp
    src/algorithms/cgr.cpp
    src/algorithms/redundancy.cpp
    src/algorithms/multicast.cpp
)
target_include_directories(sabr_algorithms PUBLIC include)
target_link_libraries(sabr_algorithms PUBLIC sabr_core)

# --- 仿真库 ---
add_library(sabr_simulation STATIC
    src/simulation/event.cpp
    src/simulation/scheduler.cpp
    src/simulation/runtime_context.cpp
    src/simulation/failure_injector.cpp
    src/io/logger.cpp
    src/simulation/traffic.cpp
    src/simulation/stats.cpp
    src/simulation/engine.cpp
)
target_include_directories(sabr_simulation PUBLIC include)
target_link_libraries(sabr_simulation PUBLIC sabr_core sabr_algorithms)

# --- IO 库 ---
add_library(sabr_io STATIC
    src/io/cli.cpp
    src/io/parser.cpp
    src/io/script_bridge.cpp
    src/io/experiment_runner.cpp
)
target_include_directories(sabr_io PUBLIC include)
target_link_libraries(sabr_io PUBLIC sabr_core sabr_algorithms sabr_simulation)

# --- 可执行文件 ---
add_executable(sabr src/main.cpp)
target_link_libraries(sabr PRIVATE sabr_core sabr_algorithms sabr_simulation sabr_io)

# --- 测试 ---
enable_testing()
include(FetchContent)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
# Windows: 防止覆盖父项目的编译器/链接器设置
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(sabr_tests
    tests/test_stub.cpp
    tests/test_models.cpp
    tests/test_dijkstra.cpp
    tests/test_yen_ksp.cpp
    tests/test_phase1.cpp
    tests/test_phase2.cpp
    tests/test_cgr.cpp
    tests/test_simulation.cpp
    tests/test_failure_injection.cpp
    tests/test_io.cpp
    tests/test_stats.cpp
    tests/test_cli.cpp
    tests/test_experiment_configs.cpp
    tests/test_reference_scenario.cpp
    tests/test_multicast.cpp
    tests/test_redundancy.cpp
    tests/test_experiment_runner.cpp
)
target_link_libraries(sabr_tests PRIVATE sabr_core sabr_algorithms sabr_simulation sabr_io GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(sabr_tests)
add_test(
    NAME sabr_python_stage9_tests
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_stage9_scripts.py
)
