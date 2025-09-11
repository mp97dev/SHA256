cmake_minimum_required(VERSION 3.10)
project(sha256_service C)
set(CMAKE_C_STANDARD 11)

find_package(OpenSSL REQUIRED)

include_directories(include)

add_executable(server src/server.c src/queue.c src/cache.c src/sha256_utils.c)
add_executable(client src/client.c src/sha256_utils.c)

target_link_libraries(server OpenSSL::Crypto pthread)
target_link_libraries(client OpenSSL::Crypto pthread) 