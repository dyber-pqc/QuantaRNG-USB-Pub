# Unit tests. Enable with -DBUILD_TESTS=ON; no hardware required.

add_executable(test_reply
    test_reply.c
    ${CMAKE_SOURCE_DIR}/src/qrng_reply.c
)

target_include_directories(test_reply PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME reply_scanner COMMAND test_reply)
