cmake_minimum_required(VERSION 3.10.0)
project (jedi)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
set(CMAKE_PDB_OUTPUT_DIRECTORY     "${CMAKE_CURRENT_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(SDL2)
add_subdirectory(freetype)
add_subdirectory(SDL2_ttf)
add_subdirectory(cpp-rrb)
add_subdirectory(jtk)
add_subdirectory(jedi)
add_subdirectory(jedi.tests)
add_subdirectory(pdcurses)
add_subdirectory(tree)
add_subdirectory(tocstr)
add_subdirectory(jsonfmt)
add_subdirectory(upper)
add_subdirectory(lower)
add_subdirectory(lexicalsort)



set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set_target_properties (SDL2 PROPERTIES FOLDER SDL2)
set_target_properties (SDL2main PROPERTIES FOLDER SDL2)
set_target_properties (SDL2-static PROPERTIES FOLDER SDL2)
set_target_properties (uninstall PROPERTIES FOLDER SDL2)
set_target_properties (SDL2_ttf PROPERTIES FOLDER SDL2)
set_target_properties (freetype PROPERTIES FOLDER SDL2)
set_target_properties (sdl_headers_copy PROPERTIES FOLDER SDL2)
set_target_properties (SDL2_test PROPERTIES FOLDER SDL2)