#include "fsw_tool.hpp"

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

void usage(const char *prog) {
    std::cout << "Full Spectrum Warrior Modding Kit (C++ CLI)\n";
    std::cout << "Usage:\n";
    std::cout << "  " << prog << " <command> [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  patch-no-downs <path-to-FSW.dll>\n";
    std::cout << "  patch-openspy <path-to-FSW.dll>\n";
    std::cout << "  patch-no-mission-failures <Chapters-dir>\n";
    std::cout << "  patch-ammo-999 <Chapters-dir>\n";
    std::cout << "  write-resolution <install-dir> <width> <height>\n";
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    std::string cmd = argv[1];
    try {
        if (cmd == "patch-no-downs") {
            if (argc != 3) {
                usage(argv[0]);
                return 1;
            }
            fs::path dll = argv[2];
            OperationResult res = fsw::patch_no_downs_limit(dll);
            if (!res.success) {
                std::cerr << res.message << "\n";
                return 2;
            }
            std::cout << res.message << "\n";
        } else if (cmd == "patch-openspy") {
            if (argc != 3) {
                usage(argv[0]);
                return 1;
            }
            fs::path dll = argv[2];
            OperationResult res = fsw::patch_gamespy_to_openspy(dll);
            if (!res.success) {
                std::cerr << res.message << "\n";
                return 2;
            }
            std::cout << res.message << "\n";
        } else if (cmd == "patch-no-mission-failures") {
            if (argc != 3) {
                usage(argv[0]);
                return 1;
            }
            fs::path chapters = argv[2];
            OperationResult res = fsw::patch_no_mission_failures(chapters);
            if (!res.success) {
                std::cerr << res.message << "\n";
                return 2;
            }
            std::cout << res.message << "\n";
        } else if (cmd == "patch-ammo-999") {
            if (argc != 3) {
                usage(argv[0]);
                return 1;
            }
            fs::path chapters = argv[2];
            OperationResult res = fsw::patch_ammo_999(chapters);
            if (!res.success) {
                std::cerr << res.message << "\n";
                return 2;
            }
            std::cout << res.message << "\n";
        } else if (cmd == "write-resolution") {
            if (argc != 5) {
                usage(argv[0]);
                return 1;
            }
            fs::path install = argv[2];
            int width = std::stoi(argv[3]);
            int height = std::stoi(argv[4]);
            OperationResult res = fsw::write_resolution(install, width, height);
            if (!res.success) {
                std::cerr << res.message << "\n";
                return 2;
            }
            std::cout << res.message << "\n";
        } else {
            usage(argv[0]);
            return 1;
        }
    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 3;
    }

    return 0;
}

