cmake_minimum_required(VERSION 4.0)
project(FileSearchSystem)

set(CMAKE_CXX_STANDARD 20)

link_libraries(ntdll shlwapi)

# Path to SDL2
# set(SDL2_DIR "C:/Libraries/SDL2")
# set(SDL2_IMAGE_DIR "C:/Libraries/SDL2_image")
# set(SDL2_TTF_DIR "C:/Libraries/SDL2_ttf")


include_directories(
#         ${CMAKE_SOURCE_DIR}/include
#         ${SDL2_DIR}/include/SDL2
#         ${SDL2_IMAGE_DIR}/include/SDL2
#         ${SDL2_TTF_DIR}/include/SDL2
)

link_directories(
#         ${SDL2_DIR}/lib
#         ${SDL2_IMAGE_DIR}/lib
#         ${SDL2_TTF_DIR}/lib
)

add_executable(FileSearchSystem
#         include/RendererSystem.h
        
        src/main.cpp
)


target_link_libraries(${PROJECT_NAME}
        mingw32

        # PRIVATE
        # ntdll
        # shlwapi
)