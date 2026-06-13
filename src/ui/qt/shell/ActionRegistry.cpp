#include "ui/qt/shell/ActionRegistry.h"

#include <algorithm>
#include <cctype>
#include <optional>

namespace beamlab::ui {

namespace {

char lower(char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

bool isWordStart(std::string_view s, std::size_t i) {
    if (i == 0) return true;
    const char p = s[i - 1];
    return p == ' ' || p == '.' || p == '-' || p == '/';
}

// Subsecuencia case-insensitive de `query` en `hay`. nullopt si no hay match.
// Score mayor = mejor: premia matches consecutivos y en inicio de palabra.
std::optional<int> fuzzyScore(std::string_view hay, std::string_view query) {
    if (query.empty()) return 0;
    int score = 0;
    std::size_t qi = 0;
    bool prevMatched = false;
    // Match greedy de izquierda a derecha (estándar en paletas tipo VS Code).
    // No busca el mejor camino posible; suficiente para queries cortas de ⌘K.
    for (std::size_t i = 0; i < hay.size() && qi < query.size(); ++i) {
        if (lower(hay[i]) == lower(query[qi])) {
            score += 10;
            if (prevMatched) score += 15;          // consecutivo
            if (isWordStart(hay, i)) score += 20;   // inicio de palabra
            prevMatched = true;
            ++qi;
        } else {
            prevMatched = false;
        }
    }
    if (qi != query.size()) return std::nullopt;
    return score;
}

} // namespace

void ActionRegistry::add(Action a) {
    actions_.push_back(std::move(a));
}

std::vector<const Action*> ActionRegistry::search(std::string_view query) const {
    struct Scored { const Action* action; int score; std::size_t order; };
    std::vector<Scored> scored;
    for (std::size_t i = 0; i < actions_.size(); ++i) {
        const auto& a = actions_[i];
        auto st = fuzzyScore(a.title, query);
        auto sc = fuzzyScore(a.category, query);
        auto best = st;
        if (sc && (!best || *sc > *best)) best = sc;
        if (best) scored.push_back({&a, *best, i});
    }
    std::stable_sort(scored.begin(), scored.end(),
        [](const Scored& x, const Scored& y) {
            if (x.score != y.score) return x.score > y.score;
            return x.order < y.order;
        });
    std::vector<const Action*> out;
    out.reserve(scored.size());
    for (const auto& s : scored) out.push_back(s.action);
    return out;
}

} // namespace beamlab::ui
