#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct OperationResult {
    bool success;
    std::string message;
};

namespace fsw {

std::vector<std::filesystem::path> iter_pak_files(const std::filesystem::path &chapters_dir);

OperationResult patch_no_downs_limit(const std::filesystem::path &dll_path);
OperationResult patch_gamespy_to_openspy(const std::filesystem::path &dll_path);
OperationResult patch_no_mission_failures(const std::filesystem::path &chapters_dir);
OperationResult patch_ammo_999(const std::filesystem::path &chapters_dir);
OperationResult write_resolution(const std::filesystem::path &install_dir, int width,
                                 int height);

} // namespace fsw

