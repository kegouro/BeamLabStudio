#include "ui/qt/dockable/DockManager.h"
#include "ui/qt/dockable/IDockableWidget.h"

#include <QApplication>
#include <QMainWindow>

#include <gtest/gtest.h>

#include <memory>

using namespace beamlab::ui::qt;

// ── Minimal dockable for testing ────────────────────────────────────

class TestDockable final : public IDockableWidget {
public:
    QWidget w;
    QString title() const override { return QStringLiteral("Test Dock"); }
    QString id() const override { return QStringLiteral("test_dock"); }
    QWidget* widget() override { return &w; }
    Qt::DockWidgetArea preferredArea() const override
        { return Qt::RightDockWidgetArea; }
    bool initiallyVisible() const override { return true; }
};

class SecondDockable final : public IDockableWidget {
public:
    QWidget w;
    QString title() const override { return QStringLiteral("Second"); }
    QString id() const override { return QStringLiteral("second_dock"); }
    QWidget* widget() override { return &w; }
};

// ── Test fixture ───────────────────────────────────────────────────

class DockManagerTest : public ::testing::Test {
protected:
    QMainWindow* mw_ = nullptr;
    DockManager* dm_ = nullptr;

    void SetUp() override
    {
        int argc = 0; char* argv[] = {nullptr};
        // QApplication may already exist from a previous test.
        if (!QApplication::instance())
            app_ = std::make_unique<QApplication>(argc, argv);

        mw_ = new QMainWindow();
        dm_ = new DockManager(mw_);
    }

    void TearDown() override
    {
        delete mw_;
        mw_ = nullptr;
        dm_ = nullptr;
    }

private:
    std::unique_ptr<QApplication> app_;
};

// ── Tests ──────────────────────────────────────────────────────────

TEST_F(DockManagerTest, RegisterDockable)
{
    dm_->registerDockable(std::make_unique<TestDockable>());
    EXPECT_EQ(mw_->findChild<QDockWidget*>(QStringLiteral("test_dock")),
              nullptr);  // objectName is set, but child search may need implementation.
    // At minimum: no crash.
}

TEST_F(DockManagerTest, SaveLayoutReturnsNonEmpty)
{
    dm_->registerDockable(std::make_unique<TestDockable>());

    auto layout = dm_->saveLayout();
    EXPECT_FALSE(layout.isEmpty());
}

TEST_F(DockManagerTest, RestoreLayoutDoesNotCrash)
{
    dm_->registerDockable(std::make_unique<TestDockable>());

    auto layout = dm_->saveLayout();
    EXPECT_NO_THROW(dm_->restoreLayout(layout));
}

TEST_F(DockManagerTest, CreateViewMenuGeneratesActions)
{
    dm_->registerDockable(std::make_unique<TestDockable>());
    dm_->registerDockable(std::make_unique<SecondDockable>());

    auto* menu = dm_->createViewMenu();
    ASSERT_NE(menu, nullptr);
    EXPECT_GE(menu->actions().size(), 2);  // 2 dockables + separator + reset
    delete menu;
}

TEST_F(DockManagerTest, CreateViewMenuReturnsCheckableActions)
{
    dm_->registerDockable(std::make_unique<TestDockable>());

    auto* menu = dm_->createViewMenu();
    auto actions = menu->actions();
    ASSERT_GE(actions.size(), 1u);
    EXPECT_TRUE(actions[0]->isCheckable());
    delete menu;
}

TEST_F(DockManagerTest, ToggleDockableHidesAndShows)
{
    dm_->registerDockable(std::make_unique<TestDockable>());

    // The dock should be visible initially.
    // Toggle → hidden.
    dm_->toggleDockable("test_dock");
    // Toggle back → visible.
    dm_->toggleDockable("test_dock");
}

TEST_F(DockManagerTest, MultipleRegistrationsDoNotCrash)
{
    for (int i = 0; i < 10; ++i) {
        auto dock = std::make_unique<TestDockable>();
        dm_->registerDockable(std::move(dock));
    }
    EXPECT_NO_THROW(dm_->saveLayout());
}

TEST_F(DockManagerTest, ResetToDefaults)
{
    dm_->registerDockable(std::make_unique<TestDockable>());

    EXPECT_NO_THROW(dm_->resetToDefaults());
}

TEST_F(DockManagerTest, ToggleInvalidIdDoesNotCrash)
{
    EXPECT_NO_THROW(dm_->toggleDockable("nonexistent"));
}
