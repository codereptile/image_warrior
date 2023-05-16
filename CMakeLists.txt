cmake_minimum_required(VERSION 3.10)
project(image_warrior)

#################################################
# General setup:
#################################################
set(CMAKE_CXX_STANDARD 20)

#################################################
# Boost setup:
#################################################

find_package(Boost REQUIRED COMPONENTS filesystem program_options)

#################################################
# Cuda setup:
#################################################

set(Torch_DIR "/usr/local/libtorch/share/cmake/Torch")
set(CAFFE2_USE_CUDNN 1)

# Find required packages
find_package(Torch REQUIRED)
find_package(OpenCV REQUIRED)

#################################################
# setup sources:
#################################################

include_directories(src)
set(
        image_warrior_sources
        src/object_database.cpp
        src/context.cpp
        src/utils.cpp
        src/processors/processor.cpp
        src/processors/image_processor.cpp
)

#################################################
# image_warrior executable:
#################################################

add_executable(image_warrior image_warrior.cpp ${image_warrior_sources})

#################################################
# Linking:
#################################################

target_link_libraries(image_warrior PRIVATE "${TORCH_LIBRARIES}" "${OpenCV_LIBS}" "${Boost_LIBRARIES}")
