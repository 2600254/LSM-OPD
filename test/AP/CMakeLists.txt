include_directories(../include)
include_directories(../include/catch)

add_executable(ap_tests
    test_ap.cpp
)

target_include_directories(ap_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/bind
    ${PROJECT_SOURCE_DIR}/include/catch
)

target_link_libraries(ap_tests PRIVATE lsm-opd profiler folly)

