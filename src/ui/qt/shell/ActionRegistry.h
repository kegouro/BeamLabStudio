#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace beamlab::ui {

struct Action {
    std::string id;
    std::string title;
    std::string category;
    std::string shortcut;
    std::function<void()> run;
};

/// Registro de acciones invocables, con búsqueda difusa para la paleta ⌘K.
/// Lógica pura — sin dependencias de Qt, testeable en aislamiento.
class ActionRegistry {
public:
    void add(Action a);
    const std::vector<Action>& all() const { return actions_; }

    /// Devuelve punteros a las acciones cuyo título/categoría hacen match
    /// difuso (subsecuencia, case-insensitive) con `query`, ordenadas por
    /// score descendente. Query vacía devuelve todas en orden de inserción.
    /// Los punteros devueltos apuntan al almacenamiento interno y se invalidan
    /// en cuanto se llama a add() (puede reubicar el vector). Úsalos antes de
    /// modificar el registro; en la paleta ⌘K las acciones se registran una
    /// vez al inicio y search() se llama después, por lo que es seguro.
    std::vector<const Action*> search(std::string_view query) const;

private:
    std::vector<Action> actions_;
};

} // namespace beamlab::ui
