#include "frontend_settings.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace nba97 {
namespace {
constexpr std::array<std::uint8_t, 14> kRuleMaximums{
    9, 9, 7, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
constexpr std::array<std::uint8_t, 11> kOptionMaximums{
    1, 9, 9, 9, 9, 9, 1, 1, 4, 2, 1};
constexpr std::array<const char*, 15> kRuleLabels{
    "defensive fouls", "offensive fouls", "foul out", "out of bounds",
    "backcourt", "traveling", "goaltending", "illegal defense",
    "3 in the key", "5 second inbounding", "10 second half court",
    "shot clock", "fatigue", "injuries", "current style"};
constexpr std::array<const char*, 11> kOptionLabels{
    "sound", "music volume", "speech volume", "SF/X volume", "crowd volume",
    "automatic replay", "keep scores close", "slow motion dunks",
    "player indicator", "display indicator", "score overlay"};

std::uint8_t wrapped(std::uint8_t value, int direction, std::uint8_t maximum) {
    const int count = static_cast<int>(maximum) + 1;
    return static_cast<std::uint8_t>((static_cast<int>(value) +
        (direction < 0 ? count - 1 : 1)) % count);
}

std::string onOff(std::uint8_t value) { return value ? "on" : "off"; }
}

FrontendSettings::FrontendSettings() {
    applyStyle(0);
    custom_rules_ = rules_;
    // FEONLY 0x80035D88 initializes the frontend settings block at
    // 0x80021D70 with these exact first-boot values.
    options_ = {1, 9, 9, 9, 9, 5, 0, 0, 3, 0, 1};
}

bool FrontendSettings::load(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) return false;
    auto parseArray = [](std::string_view value, auto& target, const auto& maximums) {
        std::size_t at = 0;
        for (std::size_t i = 0; i < target.size(); ++i) {
            const std::size_t comma = value.find(',', at);
            const std::string token(value.substr(at, comma == std::string_view::npos
                ? value.size() - at : comma - at));
            try { target[i] = static_cast<std::uint8_t>(std::clamp(
                std::stoi(token), 0, static_cast<int>(maximums[i]))); }
            catch (...) { return false; }
            if (comma == std::string_view::npos) return i + 1 == target.size();
            at = comma + 1;
        }
        return true;
    };
    bool got_rules = false, got_custom = false, got_options = false, got_style = false;
    std::uint8_t saved_style = 0;
    std::string line;
    while (std::getline(input, line)) {
        const auto equal = line.find('=');
        if (equal == std::string::npos) continue;
        const std::string key = line.substr(0, equal);
        const std::string_view value(line.data() + equal + 1, line.size() - equal - 1);
        if (key == "rules") got_rules = parseArray(value, rules_, kRuleMaximums);
        else if (key == "custom_rules") got_custom = parseArray(value, custom_rules_, kRuleMaximums);
        else if (key == "options") got_options = parseArray(value, options_, kOptionMaximums);
        else if (key == "style") {
            try {
                saved_style = static_cast<std::uint8_t>(std::clamp(std::stoi(std::string(value)), 0, 2));
                got_style = true;
            } catch (...) { return false; }
        }
    }
    if (!(got_rules && got_custom && got_options && got_style)) return false;
    style_ = saved_style;
    return true;
}

void FrontendSettings::save(const std::filesystem::path& path) const {
    std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) throw std::runtime_error("cannot write settings: " + temporary);
    auto writeArray = [&output](const char* name, const auto& values) {
        output << name << '=';
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i) output << ',';
            output << static_cast<int>(values[i]);
        }
        output << '\n';
    };
    output << "version=1\n";
    output << "style=" << static_cast<int>(style_) << '\n';
    writeArray("rules", rules_);
    writeArray("custom_rules", custom_rules_);
    writeArray("options", options_);
    output.close();
    if (!output) throw std::runtime_error("failed writing settings: " + temporary);
    std::error_code error;
    std::filesystem::remove(path, error);
    error.clear();
    std::filesystem::rename(temporary, path, error);
    if (error) throw std::runtime_error("cannot commit settings: " + error.message());
}

std::uint8_t FrontendSettings::rule(int index) const noexcept {
    return rules_[static_cast<std::size_t>(std::clamp(index, 0, 13))];
}

std::uint8_t FrontendSettings::option(int index) const noexcept {
    return options_[static_cast<std::size_t>(std::clamp(index, 0, 10))];
}

std::string FrontendSettings::ruleValue(int index) const {
    if (index < 2) return std::to_string(static_cast<int>(rule(index)) + 1);
    if (index == 2) return rule(index) == 0 ? "off" : std::to_string(rule(index) + 1);
    if (index < 14) return onOff(rule(index));
    return std::array<const char*, 3>{"arcade", "simulation", "custom"}[style_];
}

std::string FrontendSettings::optionValue(int index) const {
    const auto value = option(index);
    if (index == 0) return value ? "stereo" : "mono";
    if (index >= 1 && index <= 5) return std::to_string(static_cast<int>(value) + 1);
    if (index == 6 || index == 7 || index == 10) return onOff(value);
    if (index == 8) return std::array<const char*, 5>{
        "position", "jersey #", "position #", "name", "none"}[value];
    return std::array<const char*, 3>{
        "active player", "all players", "active team"}[value];
}

bool FrontendSettings::adjustRule(int index, int direction) noexcept {
    if (!direction || index < 0 || index > 14) return false;
    if (index == 14) {
        applyStyle(wrapped(style_, direction, 2));
        return true;
    }
    auto& value = rules_[static_cast<std::size_t>(index)];
    value = wrapped(value, direction, kRuleMaximums[static_cast<std::size_t>(index)]);
    classifyRules();
    if (style_ == 2) custom_rules_ = rules_;
    return true;
}

bool FrontendSettings::adjustOption(int index, int direction) noexcept {
    if (!direction || index < 0 || index >= static_cast<int>(options_.size())) return false;
    auto& value = options_[static_cast<std::size_t>(index)];
    value = wrapped(value, direction, kOptionMaximums[static_cast<std::size_t>(index)]);
    return true;
}

void FrontendSettings::applyStyle(std::uint8_t style) noexcept {
    style_ = std::min<std::uint8_t>(style, 2);
    if (style_ == 0) {
        rules_.fill(0);
        rules_[11] = 1;
    } else if (style_ == 1) {
        for (std::size_t i = 0; i < rules_.size(); ++i) rules_[i] = kRuleMaximums[i];
        rules_[0] = 4;
        rules_[1] = 4;
        rules_[2] = 5;
    } else {
        rules_ = custom_rules_;
    }
}

void FrontendSettings::classifyRules() noexcept {
    int sum = 0;
    for (auto value : rules_) sum += value;
    if (sum == 1 && rules_[11] == 1) style_ = 0;
    else if (sum == 24 && rules_[0] == 4 && rules_[1] == 4 && rules_[2] == 5)
        style_ = 1;
    else style_ = 2;
}

void SettingsMenu::open(FrontendPage page) noexcept {
    page_ = page;
    selected_ = 0;
    first_visible_ = 0;
}

int SettingsMenu::count() const noexcept { return page_ == FrontendPage::Rules ? 15 : 11; }
int SettingsMenu::visibleCount() const noexcept { return page_ == FrontendPage::Rules ? 6 : 7; }

bool SettingsMenu::move(int direction) noexcept {
    if (!direction) return false;
    const int previous = selected_;
    selected_ = std::clamp(selected_ + (direction < 0 ? -1 : 1), 0, count() - 1);
    if (selected_ < first_visible_) first_visible_ = selected_;
    if (selected_ >= first_visible_ + visibleCount())
        first_visible_ = selected_ - visibleCount() + 1;
    return selected_ != previous;
}

bool SettingsMenu::hover(int psx_x, int psx_y) noexcept {
    const int start_y = page_ == FrontendPage::Rules ? 87 : 72;
    const int row = (psx_y - start_y) / 16;
    if (psx_x < 85 || psx_x > 430 || psx_y < start_y || row < 0 || row >= visibleCount())
        return false;
    const int target = first_visible_ + row;
    if (target >= count() || target == selected_) return false;
    selected_ = target;
    return true;
}

const char* SettingsMenu::selectedLabel() const noexcept {
    return page_ == FrontendPage::Rules ? kRuleLabels[static_cast<std::size_t>(selected_)]
                                        : kOptionLabels[static_cast<std::size_t>(selected_)];
}

} // namespace nba97
