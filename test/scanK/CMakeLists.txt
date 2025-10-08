include_directories(../include)
include_directories(../include/catch)

add_executable(scanK
    scanK.cpp
)

target_include_directories(scanK PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/bind
    ${PROJECT_SOURCE_DIR}/include/catch
)

target_link_libraries(scanK PRIVATE lsm-opd folly profiler tcmalloc)
