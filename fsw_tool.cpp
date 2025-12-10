#include "fsw_tool.hpp"

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {

std::vector<char> read_file(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to read file: " + path.string());
    }
    return std::vector<char>(std::istreambuf_iterator<char>(in), {});
}

void write_file(const fs::path &path, const std::vector<char> &data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Failed to write file: " + path.string());
    }
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
}

void ensure_backup(const fs::path &path) {
    fs::path bak = path;
    bak += ".bak";
    if (!fs::exists(bak)) {
        fs::copy_file(path, bak);
    }
}

std::vector<char> adjust_to_original_size(const std::vector<char> &original,
                                          const std::vector<char> &modified) {
    if (modified.size() == original.size()) {
        return modified;
    }

    if (modified.size() > original.size()) {
        size_t diff = modified.size() - original.size();
        size_t i = modified.size();
        while (i > 0 && modified[i - 1] == '\0') {
            --i;
        }
        size_t nul_run = modified.size() - i;
        if (nul_run < diff) {
            throw std::runtime_error(
                "Not enough trailing NULs to keep size unchanged after patch.");
        }
        return std::vector<char>(modified.begin(), modified.end() - diff);
    }

    std::vector<char> padded = modified;
    padded.insert(padded.end(), original.size() - modified.size(), '\0');
    return padded;
}

const std::vector<std::string> AMMO_KEYS = {
    "AMMO_M203_EASY",      "AMMO_M203_HARD",       "AMMO_SMOKEGRENADE_EASY",
    "AMMO_SMOKEGRENADE_HARD", "AMMO_HANDGRENADE_EASY", "AMMO_HANDGRENADE_HARD"};

std::string patch_active_zero(const std::string &body, int &changed) {
    std::regex active_re("(?i)(active\\s*=\\s*)1\\b");
    std::string output;
    output.reserve(body.size());
    size_t last = 0;
    for (std::sregex_iterator it(body.begin(), body.end(), active_re), end; it != end;
         ++it) {
        const auto &m = *it;
        output.append(body, last, m.position() - last);
        output.append(m.str(1));
        output.push_back('0');
        last = m.position() + m.length();
        ++changed;
    }
    output.append(body, last, std::string::npos);
    return output;
}

int patch_no_mission_failures_in_pak(const fs::path &pak_path) {
    std::vector<char> bytes = read_file(pak_path);
    std::string data(bytes.begin(), bytes.end());

    std::regex block_re("(?is)\\[CLoseBehaviorDescriptor\\](.*?)/end");

    std::string output;
    output.reserve(data.size());
    size_t last = 0;
    int patched_blocks = 0;

    for (std::sregex_iterator it(data.begin(), data.end(), block_re), end; it != end;
         ++it) {
        const auto &m = *it;
        output.append(data, last, m.position() - last);
        std::string body = m.str(1);
        int changed = 0;
        std::string patched_body = patch_active_zero(body, changed);
        if (changed > 0) {
            ++patched_blocks;
        }
        output += "[CLoseBehaviorDescriptor]";
        output += patched_body;
        output += "/end";
        last = m.position() + m.length();
    }
    output.append(data, last, std::string::npos);

    if (patched_blocks > 0) {
        std::vector<char> new_bytes(output.begin(), output.end());
        new_bytes = adjust_to_original_size(bytes, new_bytes);
        ensure_backup(pak_path);
        write_file(pak_path, new_bytes);
    }

    return patched_blocks;
}

int patch_ammo_999_in_pak(const fs::path &pak_path) {
    std::vector<char> bytes = read_file(pak_path);
    std::string text(bytes.begin(), bytes.end());

    int total_changes = 0;
    for (const auto &key : AMMO_KEYS) {
        std::regex re("(?i)(" + key + R"(\s*=\s*)\d+)");
        std::string output;
        output.reserve(text.size());
        size_t last = 0;
        int changes = 0;
        for (std::sregex_iterator it(text.begin(), text.end(), re), end; it != end; ++it) {
            const auto &m = *it;
            output.append(text, last, m.position() - last);
            output.append(m.str(1));
            output += "999";
            last = m.position() + m.length();
            ++changes;
        }
        output.append(text, last, std::string::npos);
        if (changes > 0) {
            text.swap(output);
            total_changes += changes;
        }
    }

    if (total_changes > 0) {
        if (text.size() != bytes.size()) {
            throw std::runtime_error("Ammo patch changed file size; aborting.");
        }
        ensure_backup(pak_path);
        std::vector<char> out(text.begin(), text.end());
        write_file(pak_path, out);
    }
    return total_changes;
}

} // namespace

namespace fsw {

std::vector<fs::path> iter_pak_files(const fs::path &chapters_dir) {
    std::vector<fs::path> result;
    if (!fs::is_directory(chapters_dir)) {
        return result;
    }
    for (auto &entry : fs::directory_iterator(chapters_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".pak") {
            result.push_back(entry.path());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

OperationResult patch_no_downs_limit(const fs::path &dll_path) {
    static const std::vector<unsigned char> NO_DOWNS_ORIG = {
        0x8B, 0x03, 0x3B, 0x45, 0x14, 0x7E, 0x0B, 0x5E, 0x5D, 0xB0, 0x01,
        0x5B, 0x83, 0xC4, 0x08, 0xC2, 0x04, 0x00};
    static const std::vector<unsigned char> NO_DOWNS_PATCH = {
        0x8B, 0x03, 0x3B, 0x45, 0x14, 0xEB, 0x0B, 0x5E, 0x5D, 0xB0, 0x01,
        0x5B, 0x83, 0xC4, 0x08, 0xC2, 0x04, 0x00};

    try {
        std::vector<char> data = read_file(dll_path);
        auto it = std::search(data.begin(), data.end(), NO_DOWNS_ORIG.begin(),
                             NO_DOWNS_ORIG.end());
        if (it == data.end()) {
            bool already = std::search(data.begin(), data.end(), NO_DOWNS_PATCH.begin(),
                                       NO_DOWNS_PATCH.end()) != data.end();
            return {false, already ? "No downs limit patch already applied." :
                                      "Pattern not found in FSW.dll (unsupported version?)"};
        }
        ensure_backup(dll_path);
        std::copy(NO_DOWNS_PATCH.begin(), NO_DOWNS_PATCH.end(), it);
        write_file(dll_path, data);
        return {true, "No-downs limit patch applied."};
    } catch (const std::exception &ex) {
        return {false, ex.what()};
    }
}

OperationResult patch_gamespy_to_openspy(const fs::path &dll_path) {
    const std::string src = "gamespy.com";
    const std::string dst = "openspy.net"; // same length

    try {
        std::vector<char> data = read_file(dll_path);
        std::string text(data.begin(), data.end());

        size_t pos = 0;
        int replaced = 0;
        while ((pos = text.find(src, pos)) != std::string::npos) {
            text.replace(pos, src.size(), dst);
            pos += dst.size();
            replaced++;
        }
        if (replaced == 0) {
            bool already = text.find(dst) != std::string::npos;
            return {false, already ? "GameSpy -> OpenSpy already applied." :
                                     "gamespy.com not found in FSW.dll."};
        }
        ensure_backup(dll_path);
        std::vector<char> out(text.begin(), text.end());
        write_file(dll_path, out);
        std::ostringstream oss;
        oss << "Replaced " << replaced << " occurrence(s).";
        return {true, oss.str()};
    } catch (const std::exception &ex) {
        return {false, ex.what()};
    }
}

OperationResult patch_no_mission_failures(const fs::path &chapters_dir) {
    auto paks = iter_pak_files(chapters_dir);
    if (paks.empty()) {
        return {false, "No .pak files found in Chapters."};
    }

    try {
        int total_blocks = 0;
        for (const auto &pak : paks) {
            total_blocks += patch_no_mission_failures_in_pak(pak);
        }
        std::ostringstream oss;
        oss << "Patched " << total_blocks << " [CLoseBehaviorDescriptor] block(s) across "
            << paks.size() << " PAK(s).";
        return {true, oss.str()};
    } catch (const std::exception &ex) {
        return {false, ex.what()};
    }
}

OperationResult patch_ammo_999(const fs::path &chapters_dir) {
    auto paks = iter_pak_files(chapters_dir);
    if (paks.empty()) {
        return {false, "No .pak files found in Chapters."};
    }

    try {
        int total_changes = 0;
        for (const auto &pak : paks) {
            total_changes += patch_ammo_999_in_pak(pak);
        }
        std::ostringstream oss;
        oss << "Patched " << total_changes << " ammo value(s) across " << paks.size()
            << " PAK(s).";
        return {true, oss.str()};
    } catch (const std::exception &ex) {
        return {false, ex.what()};
    }
}

OperationResult write_resolution(const fs::path &install_dir, int width, int height) {
    try {
        fs::path cfg = install_dir / "Resolution.cfg";
        std::ostringstream oss;
        oss << width << ' ' << height << " 0\n";
        std::string line = oss.str();

        if (fs::exists(cfg)) {
            ensure_backup(cfg);
        }
        std::ofstream out(cfg, std::ios::binary | std::ios::trunc);
        if (!out) {
            throw std::runtime_error("Failed to write Resolution.cfg");
        }
        out << line;
        return {true, "Resolution.cfg written."};
    } catch (const std::exception &ex) {
        return {false, ex.what()};
    }
}

} // namespace fsw

