#include "ui/qt/shell/ActionRegistry.h"
#include <gtest/gtest.h>

using beamlab::ui::Action;
using beamlab::ui::ActionRegistry;

static ActionRegistry makeRegistry() {
    ActionRegistry r;
    r.add({"nav.overview", "Ir a Overview",  "Navegación", "Cmd+1", []{}});
    r.add({"nav.scene3d",  "Ir a Scene 3D",  "Navegación", "Cmd+2", []{}});
    r.add({"file.open",    "Abrir archivo",  "Archivo",    "Cmd+O", []{}});
    r.add({"export.mp4",   "Exportar MP4",   "Exportar",   "",      []{}});
    return r;
}

TEST(ActionRegistryTest, EmptyQueryReturnsAllInInsertionOrder) {
    auto r = makeRegistry();
    auto results = r.search("");
    ASSERT_EQ(results.size(), 4u);
    EXPECT_EQ(results[0]->id, "nav.overview");
    EXPECT_EQ(results[3]->id, "export.mp4");
}

TEST(ActionRegistryTest, SubstringMatchIsCaseInsensitive) {
    auto r = makeRegistry();
    auto results = r.search("scene");
    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0]->id, "nav.scene3d");
}

TEST(ActionRegistryTest, SubsequenceMatchFindsScattered) {
    auto r = makeRegistry();
    auto results = r.search("s3");
    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0]->id, "nav.scene3d");
}

TEST(ActionRegistryTest, NoMatchIsExcluded) {
    auto r = makeRegistry();
    auto results = r.search("zzzz");
    EXPECT_TRUE(results.empty());
}

TEST(ActionRegistryTest, CloserMatchRanksHigher) {
    auto r = makeRegistry();
    auto results = r.search("abrir");
    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0]->id, "file.open");
}

TEST(ActionRegistryTest, MatchesOnCategory) {
    auto r = makeRegistry();
    // "naveg" es subsecuencia de "Navegación" (categoría de nav.*) pero no
    // de "Archivo" ni "Exportar", así que sólo deben aparecer los dos nav.*.
    auto results = r.search("naveg");
    ASSERT_EQ(results.size(), 2u);
    const auto isNav = [](const beamlab::ui::Action* a) {
        return a->id == "nav.overview" || a->id == "nav.scene3d";
    };
    EXPECT_TRUE(isNav(results[0]));
    EXPECT_TRUE(isNav(results[1]));
    // file.open y export.mp4 deben estar ausentes
    for (const auto* a : results) {
        EXPECT_NE(a->id, "file.open");
        EXPECT_NE(a->id, "export.mp4");
    }
}

TEST(ActionRegistryTest, TieBreakPreservesInsertionOrder) {
    // Dos acciones con el mismo score para "zona":
    //   fuzzyScore("Zona alpha", "zona") = Z(word-start:+30) o(consec:+25)
    //                                       n(consec:+25) a(consec:+25) = 105
    //   fuzzyScore("Zona beta",  "zona") = mismo patrón = 105
    // Categoría "Test" no hace match → best == 105 para ambas.
    // Empate → orden de inserción → "zona.alpha" debe ser primero.
    ActionRegistry r;
    r.add({"zona.alpha", "Zona alpha", "Test", "", []{}});
    r.add({"zona.beta",  "Zona beta",  "Test", "", []{}});
    auto results = r.search("zona");
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0]->id, "zona.alpha");
    EXPECT_EQ(results[1]->id, "zona.beta");
}
