# Phase State — Reconstrucción BeamLabStudio

Rama integración: `integration/reconstruction`
Base: `f187a1c` (F0–F4 integrados; 114/114 headless, app bundle Qt OK)
Plan: `docs/PLAN_MAESTRO.md` (v2)

| Fase | Estado | Notas |
|------|--------|-------|
| F0  | done ✅ | higiene, build fix, −19G, gate verde |
| F1a | done ✅ | I=78 ICRU-90 + validación NIST real · 0.48% vs NIST · +3 tests |
| F2  | done ✅ | PDG carga, guards unificados, round-trip-17, goldens · +13 tests (114) |
| F3  | done ✅ | gradiente: índices, nav rail, EnergyColorMapper · neto −9 |
| F4  | done ✅ | biosim MVP end-to-end (BioSimView→Presenter→Runner), física real, app bundle OK |
| F1b | in_progress 🔨 | física biosim: straggling Bohr/Landau + shell corrections en StoppingPowerEngine/EnergyStraggling |
| F5  | in_progress 🔨 | **acotada**: borrar pipeline_ muerto, warnings→-Werror, reconectar tests, baseline perf, CI gate físico |
| F6  | pending | pulido + 7 features + docs |

## Decisión de alcance F5
Migración `io→services` + borrar `src/io` + reemplazo `ApplicationBootstrap`→`AnalysisOrchestrator` **DIFERIDA** (alto riesgo, pipeline activo funciona). F5 hace solo limpieza segura + CI + baseline. Migración = fase dedicada futura / backlog.

## Procedimiento (validado)
Agentes `isolation:worktree` + PASO 0 `git reset --hard integration/reconstruction`. Merge a integration + gate (headless+Qt) + limpiar worktree.

## Features F6 elegidas
C1 SOBP · C2 DVH · C3 LET/RBE · C4 Panel validación NIST · P1 Export científico · P2 Reporte PDF · P3 Consola Python/plugins

## Reglas de seguridad
- Merge a `main` solo al final de F6.
- No borrar `src/io` (DIFERIDO). No borrar `BioSimWidget` (legacy, desactivado en F4, conservado).
- Fase que rompe tests en integración → revert inmediato.
