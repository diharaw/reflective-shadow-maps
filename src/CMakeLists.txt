cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

find_program(CLANG_FORMAT_EXE NAMES "clang-format" DOC "Path to clang-format executable")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(RSM_SOURCES ${PROJECT_SOURCE_DIR}/src/main.cpp)
set(ASSET_SOURCES ${PROJECT_SOURCE_DIR}/data/cornell_box.obj
                  ${PROJECT_SOURCE_DIR}/data/cornell_box.mtl)

file(GLOB_RECURSE SHADER_SOURCES ${PROJECT_SOURCE_DIR}/src/*.glsl)

if(APPLE)
    add_executable(ReflectiveShadowMaps MACOSX_BUNDLE ${RSM_SOURCES} ${SHADER_SOURCES} ${ASSET_SOURCES})
    set(MACOSX_BUNDLE_BUNDLE_NAME "Reflective Shadow Maps") 
    set_source_files_properties(${SHADER_SOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/shader)
    set_source_files_properties(${ASSET_SOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
else()
    add_executable(ReflectiveShadowMaps ${RSM_SOURCES}) 
endif()

target_link_libraries(ReflectiveShadowMaps dwSampleFramework)

if (NOT APPLE)
    add_custom_command(TARGET ReflectiveShadowMaps POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/shader $<TARGET_FILE_DIR:ReflectiveShadowMaps>/shader)
    add_custom_command(TARGET ReflectiveShadowMaps POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:ReflectiveShadowMaps>)
endif()

if(CLANG_FORMAT_EXE)
    add_custom_target(clang-format-project-files COMMAND ${CLANG_FORMAT_EXE} -i -style=file ${RSM_SOURCES} ${SHADER_SOURCES})
endif()