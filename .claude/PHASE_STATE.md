# Phase State — Reconstrucción BeamLabStudio

Rama integración: `integration/reconstruction`
Base: `3305424` (F0–F5 + C1 + C4; **223 tests verde**, -Werror, app bundle Qt OK)
Plan: `docs/PLAN_MAESTRO.md` (v2) · Doc de estado: `docs/RECONSTRUCCION.md`

| Fase | Estado |
|------|--------|
| F0–F5 | done ✅ (núcleo: biosim, gradientes, rigor, pipeline, limpieza, CI) |
| F6 | in_progress 🔨 (features una-a-una con input del usuario) |

## F6 features
| # | Estado | Notas |
|---|--------|-------|
| C1 SOBP | done ✅ | panel BioSim, meseta 1.4–2.4%, +bortfeld canónico |
| C4 Panel NIST | done ✅ | tab Validación, dominio+biosim vs NIST, honesto sin inventar datos |
| C2 DVH | pending | histograma dosis-volumen |
| C3 LET/RBE | pending | colormap, EnergyScaleConverter |
| P1 Export científico | pending | Parquet/ROOT + DICOM-RT |
| P2 Reporte PDF | pending | skill pdf |
| P3 Consola Python/plugins | pending | pybeamlab / IPlugin |
| Disclaimers | pending | 4 frentes (UI/CLI/reports/logs) |

## Routing de modelos (opus en límite de sesión)
Agentes ruteados a **sonnet** (implementación) / **haiku** (tareas simples). Fixes de física complejos: opus cuando esté disponible.

## Procedimiento (validado)
Agentes `isolation:worktree` + PASO 0 `git reset --hard integration/reconstruction`. Merge ff + gate (headless+Qt) + limpiar worktree. Brief con regla anti-rabbit-hole donde aplique.

## Reglas de seguridad
- Merge a `main` solo al final de F6.
- No borrar `src/io` (DIFERIDO). No borrar `BioSimWidget` (conservado).
- Fase que rompe tests → revert inmediato.

## Estado: detenido esperando instrucciones del usuario.
