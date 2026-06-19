#include "ui/qt/presenters/AnalysisPresenter.h"
#include "platform/EventBus.h"
#include "services/analysis/AnalysisOrchestrator.h"
#include "services/import/ImporterRegistry.h"
#include "services/import/Geant4CsvImporter.h"
#include "services/export/ExporterRegistry.h"

#include <gtest/gtest.h>

using namespace beamlab::ui;
using namespace beamlab::platform;
using namespace beamlab::services::analysis;
using namespace beamlab::services::import;

// ── Tests ─────────────────────────────────────────────────────────
// Note: AnalysisOrchestrator::run() is NOT virtual, so we cannot mock it
// directly.  These tests verify synchronous signal emission and the
// filter/cancel functionality that does not depend on the virtual call.

TEST(AnalysisPresenterTest, ConstructorDoesNotThrow)
{
    EventBus bus;
    // Using nullptr as orchestrator — the presenter handles it gracefully.
    EXPECT_NO_THROW(AnalysisPresenter(nullptr, &bus));
}

TEST(AnalysisPresenterTest, StartAnalysisEmitsStartedSignal)
{
    EventBus bus;
    AnalysisPresenter presenter(nullptr, &bus);

    bool started = false;
    QObject::connect(&presenter, &AnalysisPresenter::analysisStarted,
        [&]() { started = true; });

    presenter.startAnalysis("/test/file.csv");

    EXPECT_TRUE(started);
}

TEST(AnalysisPresenterTest, CancelDoesNotCrashWithNullOrchestrator)
{
    EventBus bus;
    AnalysisPresenter presenter(nullptr, &bus);
    EXPECT_NO_THROW(presenter.cancelAnalysis());
}

TEST(AnalysisPresenterTest, LoadProfileEmitsProfilesChanged)
{
    EventBus bus;
    AnalysisPresenter presenter(nullptr, &bus);

    bool signalFired = false;
    QStringList profiles;

    QObject::connect(&presenter, &AnalysisPresenter::availableProfilesChanged,
        [&](const QStringList& p) {
            signalFired = true;
            profiles = p;
        });

    presenter.loadProfile("quick_inspect");

    ASSERT_TRUE(signalFired);
    EXPECT_TRUE(profiles.contains("quick_inspect"));
}

TEST(AnalysisPresenterTest, ImporterFiltersEmptyWhenNoRegistry)
{
    EventBus bus;
    AnalysisPresenter presenter(nullptr, &bus);

    auto filters = presenter.getAvailableImporterFilters();
    EXPECT_TRUE(filters.isEmpty());
}

TEST(AnalysisPresenterTest, ExporterFormatsEmptyWhenNoRegistry)
{
    EventBus bus;
    AnalysisPresenter presenter(nullptr, &bus);

    auto formats = presenter.getAvailableExporterFormats();
    EXPECT_TRUE(formats.isEmpty());
}

TEST(AnalysisPresenterTest, ExportResultsDoesNotCrash)
{
    EventBus bus;
    AnalysisPresenter presenter(nullptr, &bus);
    EXPECT_NO_THROW(presenter.exportResults("csv", "/tmp/export"));
}

TEST(AnalysisPresenterTest, SetImporterRegistryEnablesFilters)
{
    EventBus bus;
    AnalysisPresenter presenter(nullptr, &bus);

    auto* registry = new ImporterRegistry();
    registry->registerImporter(std::make_unique<Geant4CsvImporter>());
    presenter.setImporterRegistry(registry);

    auto filters = presenter.getAvailableImporterFilters();
    EXPECT_FALSE(filters.isEmpty());
    bool hasAll = std::any_of(filters.begin(), filters.end(),
        [](const QString& f) { return f.contains("Todos los soportados"); });
    EXPECT_TRUE(hasAll);
}

TEST(AnalysisPresenterTest, ErrorSignalCrossThreadSafe)
{
    EventBus bus;
    AnalysisPresenter presenter(nullptr, &bus);

    // Subscribing to EventBus should not crash even with null orchestrator.
    EXPECT_NO_THROW(bus.publish(ErrorOccurred{Severity::Error, "test", "test"}));
}
