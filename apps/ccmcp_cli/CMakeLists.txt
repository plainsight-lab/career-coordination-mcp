add_executable(ccmcp_cli main.cpp)
target_link_libraries(ccmcp_cli PRIVATE ccmcp nlohmann_json::nlohmann_json)
