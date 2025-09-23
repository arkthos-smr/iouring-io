cmake_minimum_required(VERSION 3.22)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(arkthos-iouring)

set(CMAKE_C_COMPILER /usr/bin/clang-21)
set(CMAKE_CXX_COMPILER /usr/bin/clang++-21)

# Set flags using the proper CMake variables
add_compile_options(-O2 -stdlib=libc++)
add_link_options(-stdlib=libc++ -L/usr/lib/llvm-21/lib -lc++ -lc++abi)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SOURCES "src/*.cpp")
add_executable(arkthos-iouring ${SOURCES})

# Use the sanitized headers you installed from the full kernel source
set(KERNEL_HEADERS_SANITIZED /usr/include/linux-6.16.7/include)

target_include_directories(arkthos-iouring PRIVATE
        ${KERNEL_HEADERS_SANITIZED}
        # You can add any other includes your project needs here
)

target_compile_options(arkthos-iouring PRIVATE
        -Wno-reorder-init-list
        # or: -Wno-designated-init
)
