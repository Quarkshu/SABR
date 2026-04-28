#pragma once

#include <filesystem>
#include <string>
#include <vector>

std::filesystem::path find_workspace_root(const std::filesystem::path& start_path);
std::filesystem::path find_python_executable(const std::filesystem::path& workspace_root);
std::filesystem::path find_script_path(const std::filesystem::path& workspace_root,
                                       const std::string& script_name);
int run_python_script(const std::filesystem::path& workspace_root,
                      const std::string& script_name,
                      const std::vector<std::string>& arguments);