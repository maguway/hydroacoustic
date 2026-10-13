# common — общая библиотека (без GUI, без LibTorch)
add_library(common STATIC
        uca_geometry.cpp
)

target_include_directories(common PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Eigen — header-only, не используем его CMake (старый, несовместим с CMake 4.x).
# Просто клонируем/скачиваем исходники и добавляем include-путь.
FetchContent_Declare(eigen
        GIT_REPOSITORY https://github.com/eigenteam/eigen-git-mirror.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/vendor/eigen-src
        BINARY_DIR ${CMAKE_SOURCE_DIR}/vendor/eigen-bin
        SUBBUILD_DIR ${CMAKE_SOURCE_DIR}/vendor/eigen-sub
)
FetchContent_Populate(eigen)

target_include_directories(common PUBLIC ${eigen_SOURCE_DIR})