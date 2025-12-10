#pragma once

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

struct OperationResult {
    bool success;
    std::string message;
};

namespace fsw {

struct DescriptorRecord {
    std::size_t index{};
    std::string desc_type;
    std::size_t start{};
    std::size_t end{};
    std::string body; // bytes between header and /end
    std::map<std::string, std::string> key_values;
};

class DescriptorModel {
  public:
    explicit DescriptorModel(const std::filesystem::path &pak_path);

    const std::filesystem::path &path() const { return pak_path_; }
    const std::vector<DescriptorRecord> &descriptors() const { return descriptors_; }
    std::set<std::string> options_for_key(const std::string &key) const;

    void replace_key(DescriptorRecord &rec, const std::string &key,
                     const std::string &new_value);
    void save();

  private:
    void parse();
    std::filesystem::path pak_path_;
    std::vector<char> original_;
    std::vector<DescriptorRecord> descriptors_;
    std::map<std::string, std::set<std::string>> options_;
};

struct RuleField {
    std::string key;
    std::string value;
    std::string prefix;
    std::string newline;
    std::size_t line_index{};
};

struct RuleSection {
    std::string name;
    std::vector<std::string> fields_order;
    std::map<std::string, RuleField> fields;
};

class RulesModel {
  public:
    explicit RulesModel(const std::filesystem::path &pak_path);

    const std::filesystem::path &path() const { return pak_path_; }
    const std::vector<RuleSection> &sections() const { return sections_; }

    void update_field(RuleField &field, const std::string &new_value);
    RuleField *find_field(const std::string &section, const std::string &key);
    std::vector<char> build_new_data() const;
    void save();

    std::size_t rules_start() const { return rules_start_; }
    std::size_t rules_end() const { return rules_end_; }
    const std::vector<char> &raw_data() const { return data_; }

  private:
    void parse();
    std::pair<std::size_t, std::size_t> locate_rules_region() const;

    std::filesystem::path pak_path_;
    std::vector<char> data_;
    std::vector<std::string> lines_;
    std::vector<RuleSection> sections_;
    std::map<std::string, RuleSection *> section_map_;
    std::size_t rules_start_{};
    std::size_t rules_end_{};
};

std::vector<std::filesystem::path> iter_pak_files(const std::filesystem::path &chapters_dir);

OperationResult patch_no_downs_limit(const std::filesystem::path &dll_path);
OperationResult patch_gamespy_to_openspy(const std::filesystem::path &dll_path);
OperationResult patch_no_mission_failures(const std::filesystem::path &chapters_dir);
OperationResult patch_ammo_999(const std::filesystem::path &chapters_dir);
OperationResult write_resolution(const std::filesystem::path &install_dir, int width,
                                 int height);

} // namespace fsw

