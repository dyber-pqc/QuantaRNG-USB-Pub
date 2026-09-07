add_executable(qrng-basic   basic.c)
add_executable(qrng-stream  stream.c)
add_executable(qrng-info    info.c)

foreach(t qrng-basic qrng-stream qrng-info)
    target_link_libraries(${t} PRIVATE quantarng)
endforeach()
