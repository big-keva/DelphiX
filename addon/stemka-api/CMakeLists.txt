project(stemka_api)

add_compile_options(-static-libstdc++)

add_library(stemka-api SHARED stemka-api.cpp)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")

target_link_libraries(stemka-api stemka moonycode)

add_subdirectory(stemka)
add_subdirectory(tests)
