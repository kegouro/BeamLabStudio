# Phase State — Reconstrucción BeamLabStudio

Rama integración: `integration/reconstruction`
Base: `a6cb4ff` (F0–F5 integrados; **212 tests verde**, -Werror, CI, baseline, app bundle Qt OK)
Plan: `docs/PLAN_MAESTRO.md` (v2)

| Fase | Estado | Notas |
|------|--------|-------|
| F0  | done ✅ | higiene, build fix, −19G |
| F1a | done ✅ | dominio: I=78 ICRU-90 + validación NIST real · 0.48% vs NIST |
| F1b | done ✅ | biosim física: StoppingPowerEngine ya 0.95% (no tocado); arreglados 2 bugs straggling (Bohr β², Landau bias) · +5 tests |
| F2  | done ✅ | PDG, guards unificados, round-trip-17, goldens · +13 tests |
| F3  | done ✅ | gradiente: índices, nav rail, EnergyColorMapper · neto −9 |
| F4  | done ✅ | biosim MVP end-to-end, física real, app bundle |
| F5  | done ✅ | pipeline_ borrado, 0 warnings + -Werror, 114→207 tests reconectados, baseline.json, ci.yml |
| F6  | pending | pulido + 7 features + docs |

Total tests: **212** (2 skipped = plugins .so intencionales).

## Migración DIFERIDA (backlog)
io→services + borrar src/io + ApplicationBootstrap→AnalysisOrchestrator. Alto riesgo, pipeline activo funciona. Fase dedicada futura.

## Features F6 elegidas
C1 SOBP · C2 DVH · C3 LET/RBE · C4 Panel validación NIST · P1 Export científico · P2 Reporte PDF · P3 Consola Python/plugins

## Procedimiento (validado)
Agentes `isolation:worktree` + PASO 0 `git reset --hard integration/reconstruction`. Merge + gate (headless+Qt) + limpiar worktree.

## Reglas de seguridad
- Merge a `main` solo al final de F6.
- No borrar `src/io` (DIFERIDO). No borrar `BioSimWidget` (conservado).
- Fase que rompe tests → revert inmediato.
