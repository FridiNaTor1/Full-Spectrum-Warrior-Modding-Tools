#include "fsw_tool.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string_view>

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
    try {
        fsw::RulesModel model(pak_path);
        int changed = 0;
        for (const auto &sec : model.sections()) {
            for (const auto &key : sec.fields_order) {
                if (std::find(AMMO_KEYS.begin(), AMMO_KEYS.end(), key) != AMMO_KEYS.end()) {
                    if (auto *field = model.find_field(sec.name, key)) {
                        if (field->value != "999") {
                            model.update_field(*field, "999");
                            ++changed;
                        }
                    }
                }
            }
        }
        if (changed > 0) {
            std::vector<char> new_bytes = model.build_new_data();
            ensure_backup(pak_path);
            write_file(pak_path, new_bytes);
        }
        return changed;
    } catch (...) {
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
}

} // namespace

namespace fsw {

DescriptorModel::DescriptorModel(const fs::path &pak_path) : pak_path_(pak_path) { parse(); }

std::set<std::string> DescriptorModel::options_for_key(const std::string &key) const {
    auto it = options_.find(key);
    if (it == options_.end()) {
        return {};
    }
    return it->second;
}

void DescriptorModel::parse() {
    original_ = read_file(pak_path_);
    std::string data(original_.begin(), original_.end());

    std::regex desc_re(R"((?s)\[(C\w+Descriptor)\](.*?)/end)");
    std::regex kv_re(
        "([A-Za-z0-9_]+)\\s*=\\s*(?:\"([^\"]*?)\"|([^\\s\\r\\n;\"\\x00]+))");

    std::size_t index = 0;
    for (std::sregex_iterator it(data.begin(), data.end(), desc_re), end; it != end; ++it) {
        const auto &m = *it;
        DescriptorRecord rec;
        rec.index = index++;
        rec.desc_type = m.str(1);
        rec.start = static_cast<std::size_t>(m.position());
        rec.end = rec.start + static_cast<std::size_t>(m.length());
        rec.body = m.str(2);

        std::smatch kv_match;
        std::string body = rec.body;
        for (std::sregex_iterator kv_it(body.begin(), body.end(), kv_re), kv_end; kv_it != kv_end;
             ++kv_it) {
            const auto &km = *kv_it;
            std::string raw_key = km.str(1);
            if (!raw_key.empty()) {
                raw_key[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(raw_key[0])));
            }
            std::string val = km.str(2).empty() ? km.str(3) : km.str(2);
            rec.key_values[raw_key] = val;
            options_[raw_key].insert(val);
        }

        descriptors_.push_back(std::move(rec));
    }
}

void DescriptorModel::replace_key(DescriptorRecord &rec, const std::string &key,
                                  const std::string &new_value) {
    std::string needle = key;
    std::string key_bytes = key;
    std::regex re("(?i)(" + key_bytes + R"(\s*=\s*)(?:"[^"]*"|[^\s\r\n;"\x00]+))");

    std::string body = rec.body;
    std::string replacement;
    if (new_value.empty() || new_value.find(' ') != std::string::npos ||
        new_value.find('=') != std::string::npos) {
        replacement = "$1\"" + new_value + "\"";
    } else {
        replacement = "$1" + new_value;
    }

    std::string new_body;
    int replacements = 0;
    new_body.reserve(body.size());
    std::size_t last = 0;
    for (std::sregex_iterator it(body.begin(), body.end(), re), end; it != end; ++it) {
        const auto &m = *it;
        new_body.append(body, last, m.position() - last);
        std::string prefix = m.str(1);
        if (new_value.empty() || new_value.find(' ') != std::string::npos ||
            new_value.find('=') != std::string::npos) {
            new_body += prefix + '"' + new_value + '"';
        } else {
            new_body += prefix + new_value;
        }
        last = m.position() + m.length();
        ++replacements;
        break; // only first occurrence
    }
    new_body.append(body, last, std::string::npos);

    if (replacements == 0) {
        if (new_value.empty() || new_value.find(' ') != std::string::npos ||
            new_value.find('=') != std::string::npos) {
            new_body = body + "\n" + key + " = \"" + new_value + "\"\n";
        } else {
            new_body = body + "\n" + key + " = " + new_value + "\n";
        }
    }

    rec.body = new_body;
    rec.key_values[key] = new_value;
    options_[key].insert(new_value);
}

void DescriptorModel::save() {
    std::string rebuilt;
    rebuilt.reserve(original_.size());

    std::string data(original_.begin(), original_.end());
    std::size_t pos = 0;
    for (const auto &rec : descriptors_) {
        rebuilt.append(data, pos, rec.start - pos);
        rebuilt += '[' + rec.desc_type + ']';
        rebuilt += rec.body;
        rebuilt += "/end";
        pos = rec.end;
    }
    rebuilt.append(data, pos, std::string::npos);

    std::vector<char> new_bytes(rebuilt.begin(), rebuilt.end());
    new_bytes = adjust_to_original_size(original_, new_bytes);
    ensure_backup(pak_path_);
    write_file(pak_path_, new_bytes);
}

RulesModel::RulesModel(const fs::path &pak_path) : pak_path_(pak_path) {
    data_ = read_file(pak_path_);
    auto [start, end] = locate_rules_region();
    if (start == static_cast<std::size_t>(-1) || end == static_cast<std::size_t>(-1)) {
        throw std::runtime_error("Could not find [AI]/[Health] rules region.");
    }
    rules_start_ = start;
    rules_end_ = end;

    std::string region(data_.begin() + static_cast<std::ptrdiff_t>(rules_start_),
                      data_.begin() + static_cast<std::ptrdiff_t>(rules_end_));
    std::stringstream ss(region);
    std::string line;
    while (std::getline(ss, line)) {
        lines_.push_back(line + '\n');
    }
    parse();
}

std::pair<std::size_t, std::size_t> RulesModel::locate_rules_region() const {
    std::string_view view(data_.data(), data_.size());
    std::size_t start = view.find("[AI]");
    if (start == std::string::npos) {
        return {static_cast<std::size_t>(-1), static_cast<std::size_t>(-1)};
    }
    std::size_t health = view.find("[Health]", start);
    if (health == std::string::npos) {
        return {static_cast<std::size_t>(-1), static_cast<std::size_t>(-1)};
    }
    std::string nul_run(10, '\0');
    std::size_t pad_pos = view.find(nul_run, health);
    std::size_t end = pad_pos == std::string::npos ? view.size() : pad_pos;
    return {start, end};
}

void RulesModel::parse() {
    std::regex header_re(R"([\s\x00]*\[([^\]]+)\]\s*)");
    std::regex kv_re(R"((\s*([A-Za-z0-9_]+)\s*=\s*)(.*?)(\r?\n?)$)");

    RuleSection *current = nullptr;
    for (std::size_t idx = 0; idx < lines_.size(); ++idx) {
        const std::string &line = lines_[idx];
        std::smatch m;
        if (std::regex_match(line, m, header_re)) {
            std::string name = m.str(1);
            auto it = section_map_.find(name);
            if (it == section_map_.end()) {
                sections_.push_back({name, {}, {}});
                section_map_[name] = &sections_.back();
                current = &sections_.back();
            } else {
                current = it->second;
            }
            continue;
        }
        if (!current) {
            continue;
        }
        if (std::regex_match(line, m, kv_re)) {
            RuleField field;
            field.prefix = m.str(1);
            field.key = m.str(2);
            field.value = m.str(3);
            field.newline = m.str(4);
            field.line_index = idx;
            current->fields[field.key] = field;
            current->fields_order.push_back(field.key);
        }
    }
}

void RulesModel::update_field(RuleField &field, const std::string &new_value) {
    field.value = new_value;
}

RuleField *RulesModel::find_field(const std::string &section, const std::string &key) {
    auto it = section_map_.find(section);
    if (it == section_map_.end()) {
        return nullptr;
    }
    auto field_it = it->second->fields.find(key);
    if (field_it == it->second->fields.end()) {
        return nullptr;
    }
    return &field_it->second;
}

std::vector<char> RulesModel::build_new_data() const {
    std::vector<std::string> new_lines = lines_;
    for (const auto &sec : sections_) {
        for (const auto &key : sec.fields_order) {
            const auto &field = sec.fields.at(key);
            std::string prefix = field.prefix.empty() ? field.key + " = " : field.prefix;
            std::string newline = field.newline;
            new_lines[field.line_index] = prefix + field.value + newline;
        }
    }

    std::string new_region;
    for (const auto &l : new_lines) {
        new_region += l;
    }

    const std::vector<char> &orig = data_;
    std::size_t orig_len = orig.size();
    std::size_t old_region_len = rules_end_ - rules_start_;

    std::size_t pad_start = rules_end_;
    std::size_t pad_end = pad_start;
    while (pad_end < orig_len && orig[pad_end] == '\0') {
        ++pad_end;
    }
    std::size_t pad_len = pad_end - pad_start;

    std::vector<char> output;
    output.reserve(orig_len);
    output.insert(output.end(), orig.begin(), orig.begin() + static_cast<std::ptrdiff_t>(rules_start_));
    output.insert(output.end(), new_region.begin(), new_region.end());

    std::ptrdiff_t diff = static_cast<std::ptrdiff_t>(new_region.size()) -
                          static_cast<std::ptrdiff_t>(old_region_len);
    if (pad_len > 0) {
        if (diff > 0) {
            if (static_cast<std::size_t>(diff) > pad_len) {
                throw std::runtime_error("Not enough NUL padding after rules to expand.");
            }
            output.insert(output.end(), pad_len - static_cast<std::size_t>(diff), '\0');
        } else {
            output.insert(output.end(), pad_len + static_cast<std::size_t>(-diff), '\0');
        }
    }

    output.insert(output.end(), orig.begin() + static_cast<std::ptrdiff_t>(pad_end), orig.end());

    if (output.size() != orig_len) {
        throw std::runtime_error("Rules rebuild changed file size unexpectedly.");
    }

    return output;
}

void RulesModel::save() {
    std::vector<char> new_bytes = build_new_data();
    ensure_backup(pak_path_);
    write_file(pak_path_, new_bytes);
}

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

