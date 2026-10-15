# simulator
add_executable(simulator
        main.cpp
        core/source_model.cpp
        core/propagation.cpp
        core/scene.cpp
        core/ocean_noise.cpp
        core/dolphin_whistle_test.cpp
        dataset/generator.cpp
)

target_include_directories(simulator PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/core
)

target_link_libraries(simulator PRIVATE common)