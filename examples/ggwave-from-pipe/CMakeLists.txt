set(TARGET ggwave-from-pipe)

add_executable(${TARGET} main.cpp)

target_include_directories(${TARGET} PRIVATE
    ..
    )

target_link_libraries(${TARGET} PRIVATE
    ggwave
    ggwave-common
    ${CMAKE_THREAD_LIBS_INIT}
    )
