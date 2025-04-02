# 设置CMake最低版本要求
cmake_minimum_required(VERSION 3.10.0)

# 设置项目名称
project(EncodeTest)
# 添加 include 目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# 添加 lib 目录
link_directories(${CMAKE_SOURCE_DIR}/lib)

# 添加可执行文件
add_executable(encode_test src/main.cpp)
# 链接 libmultimedia.so 库
target_link_libraries(encode_test multimedia)