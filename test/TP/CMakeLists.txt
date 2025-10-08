include_directories(../include)
include_directories(../include/catch)
include_directories(../include/folly)

add_executable(tp_tests
    test_tp.cpp
)

target_include_directories(tp_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/bind
    ${PROJECT_SOURCE_DIR}/include/catch
)

target_link_libraries(tp_tests PRIVATE lsm-opd folly)

