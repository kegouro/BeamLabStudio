#include "core/config/ProfileManager.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace beamlab::core;

// ── Fixture ─────────────────────────────────────────────────────────

class ProfileManagerTest : public ::testing::Test {
protected:
    fs::path profilesDir_;
    fs::path defaultConfig_;
    std::unique_ptr<ProfileManager> pm_;

    void SetUp() override
    {
        tmpDir_ = fs::temp_directory_path() / "beamlab_profile_test";
        fs::create_directories(tmpDir_);
        profilesDir_ = tmpDir_ / "profiles";
        fs::create_directories(profilesDir_);
        defaultConfig_ = tmpDir_ / "default_analysis.json";
        writeJson(defaultConfig_, R"({
            "storage": { "sqlite_threshold_mb": 100, "batch_size": 100000 },
            "analysis": { "binning": { "axial_bins": 501 } },
            "export": { "mp4": { "framerate": 30 } }
        })");
        pm_ = std::make_unique<ProfileManager>(
            profilesDir_.string(), defaultConfig_.string());
    }

    void TearDown() override { fs::remove_all(tmpDir_); }

    void writeJson(const fs::path& path, const std::string& content)
    {
        std::ofstream out(path);
        out << content;
        out.close();
    }

    std::string readFile(const fs::path& path)
    {
        std::ifstream in(path);
        return std::string((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
    }

    fs::path tmpDir_;
};

// ── Available profiles ──────────────────────────────────────────────

TEST_F(ProfileManagerTest, AvailableProfilesEmptyByDefault)
{
    auto profiles = pm_->availableProfiles();
    EXPECT_TRUE(profiles.empty());
}

TEST_F(ProfileManagerTest, AvailableProfilesListsJsonFiles)
{
    writeJson(profilesDir_ / "quick_inspect.json", "{}");
    writeJson(profilesDir_ / "full_analysis.json", "{}");

    auto profiles = pm_->availableProfiles();
    EXPECT_EQ(profiles.size(), 2u);
    EXPECT_NE(std::find(profiles.begin(), profiles.end(), "quick_inspect"),
              profiles.end());
}

// ── Load profile ───────────────────────────────────────────────────

TEST_F(ProfileManagerTest, LoadProfileReturnsOverrides)
{
    writeJson(profilesDir_ / "quick_inspect.json",
        R"({ "analysis": { "binning": { "axial_bins": 101 } } })");

    auto profile = pm_->loadProfile("quick_inspect");
    EXPECT_EQ(profile["analysis"]["binning"]["axial_bins"], 101);
}

TEST_F(ProfileManagerTest, LoadMissingProfileReturnsEmptyObject)
{
    auto profile = pm_->loadProfile("nonexistent");
    EXPECT_TRUE(profile.is_object());
    EXPECT_TRUE(profile.empty());
}

// ── Resolve: merge hierarchy ───────────────────────────────────────

TEST_F(ProfileManagerTest, ResolveReturnsDefaultWhenNoProfile)
{
    auto cfg = pm_->resolve("nonexistent");
    EXPECT_EQ(cfg["storage"]["sqlite_threshold_mb"], 100);
    EXPECT_EQ(cfg["analysis"]["binning"]["axial_bins"], 501);
}

TEST_F(ProfileManagerTest, ResolveMergesProfileOverDefault)
{
    writeJson(profilesDir_ / "quick_inspect.json",
        R"({ "analysis": { "binning": { "axial_bins": 101 } } })");

    auto cfg = pm_->resolve("quick_inspect");
    EXPECT_EQ(cfg["analysis"]["binning"]["axial_bins"], 101);
    EXPECT_EQ(cfg["storage"]["sqlite_threshold_mb"], 100); // inherited from default
}

TEST_F(ProfileManagerTest, ResolveCLIOverridesEverything)
{
    writeJson(profilesDir_ / "quick_inspect.json",
        R"({ "analysis": { "binning": { "axial_bins": 101 } } })");

    nlohmann::json cliOverrides;
    cliOverrides["analysis"]["binning"]["axial_bins"] = 999;

    auto cfg = pm_->resolve("quick_inspect", cliOverrides);
    EXPECT_EQ(cfg["analysis"]["binning"]["axial_bins"], 999);
}

TEST_F(ProfileManagerTest, ResolveDeepMergeObjects)
{
    writeJson(profilesDir_ / "full_analysis.json",
        R"({ "export": { "mp4": { "frame_count": 240 } } })");

    auto cfg = pm_->resolve("full_analysis");
    // Default values should still be present where profile didn't override.
    EXPECT_EQ(cfg["export"]["mp4"]["frame_count"], 240);
    EXPECT_EQ(cfg["export"]["mp4"]["framerate"], 30); // inherited
}

// ── Malformed JSON ─────────────────────────────────────────────────

TEST_F(ProfileManagerTest, MalformedProfileYieldsEmptyObject)
{
    writeJson(profilesDir_ / "broken.json", "{ this is not valid json !!!");

    auto profile = pm_->loadProfile("broken");
    EXPECT_FALSE(profile.is_discarded());
    EXPECT_TRUE(profile.is_object());
    EXPECT_TRUE(profile.empty());
}

TEST_F(ProfileManagerTest, ResolveWithMalformedProfileFallsBackToDefaults)
{
    writeJson(profilesDir_ / "broken.json", R"({ "analysis": { unterminated)");

    auto cfg = pm_->resolve("broken");
    EXPECT_FALSE(cfg.is_discarded());
    EXPECT_EQ(cfg["analysis"]["binning"]["axial_bins"], 501);
    EXPECT_EQ(cfg["storage"]["sqlite_threshold_mb"], 100);
}

// ── Save user overrides ────────────────────────────────────────────

TEST_F(ProfileManagerTest, SaveUserOverridesCreatesFile)
{
    nlohmann::json overrides;
    overrides["ui"]["log_max_lines"] = 5000;

    pm_->saveUserOverrides("quick_inspect", overrides);

    auto userConfigPath = fs::path(
        ProfileManager::userConfigDir()) / "quick_inspect.json";
    ASSERT_TRUE(fs::exists(userConfigPath));
    auto content = readFile(userConfigPath);

    EXPECT_TRUE(content.find("5000") != std::string::npos);

    // Cleanup.
    fs::remove(userConfigPath);
}

// ── Resolve with user overrides ────────────────────────────────────

TEST_F(ProfileManagerTest, ResolveAppliesUserOverrides)
{
    writeJson(profilesDir_ / "quick_inspect.json",
        R"({ "analysis": { "binning": { "axial_bins": 101 } } })");

    // Save a user override.
    nlohmann::json userOverrides;
    userOverrides["analysis"]["focus"]["metric"] = "custom_metric";
    pm_->saveUserOverrides("quick_inspect", userOverrides);

    auto cfg = pm_->resolve("quick_inspect");

    // User override should be present.
    EXPECT_EQ(cfg["analysis"]["focus"]["metric"], "custom_metric");
    // Profile value should still be inherited.
    EXPECT_EQ(cfg["analysis"]["binning"]["axial_bins"], 101);

    auto userConfigPath = fs::path(
        ProfileManager::userConfigDir()) / "quick_inspect.json";
    fs::remove(userConfigPath);
}
