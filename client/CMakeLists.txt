cmake_minimum_required(VERSION 3.24)
set(CMAKE_CXX_STANDARD 17)
project(client1)
# sources
set(source_files
        main.cpp
        )

# build
link_libraries(ws2_32 wsock32)
link_libraries(stdc++fs)
add_executable(${CMAKE_PROJECT_NAME} ${source_files})
target_link_libraries(${CMAKE_PROJECT_NAME} ws2_32)


add_executable(client main.cpp)
