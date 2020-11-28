message(STATUS "MuseDev CMake build wrapper")
set(_repo_root "${CMAKE_SOURCE_DIR}/../..")
execute_process(
    COMMAND ${CMAKE_COMMAND} -Bbuild -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -G "${CMAKE_GENERATOR}" .
    WORKING_DIRECTORY "${_repo_root}"
)
message(STATUS "CMake configure done")

# Copy compile_commands.json to the place MuseDev expects it
execute_process(COMMAND ${CMAKE_COMMAND} -E cat "${_repo_root}/build/compile_commands.json")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy "${_repo_root}/build/compile_commands.json" "${_repo_root}/compile_commands.json")

message(STATUS "CMake post-configure steps done")

return()
