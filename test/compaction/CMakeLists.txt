include_directories(../include)
include_directories(../include/catch)

add_executable(compaction_tests
    test_compaction.cpp
)

target_include_directories(compaction_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/bind
    ${PROJECT_SOURCE_DIR}/include/catch
)

target_link_libraries(compaction_tests PRIVATE lsm-opd folly profiler tcmalloc)

