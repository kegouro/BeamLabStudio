# Phase State — Reconstrucción BeamLabStudio

Rama integración: `integration/reconstruction`
Base: `dcf15d3` (F0–F5 + C1; **217 tests verde**, -Werror, app bundle Qt OK)
Plan: `docs/PLAN_MAESTRO.md` (v2)

| Fase | Estado | Notas |
|------|--------|-------|
| F0–F5 | done ✅ | reconstrucción núcleo (ver historial git) |
| F6 | in_progress 🔨 | features una-a-una con input del usuario |

## F6 features
| # | Estado | |
|---|--------|--|
| C1 SOBP | done ✅ | panel en tab BioSim, meseta 1.4–2.4%, +bortfeld canónico (Bortfeld 1997 PCF) |
| C2 DVH | pending | histograma dosis-volumen |
| C3 LET/RBE | pending | colormap, reusa EnergyScaleConverter |
| C4 Panel NIST | pending | tabla dE/dx vs NIST en vivo, reusa F1a/F1b |
| P1 Export científico | pending | Parquet/ROOT (flags) + DICOM-RT |
| P2 Reporte PDF | pending | skill pdf |
| P3 Consola Python/plugins | pending | pybeamlab / IPlugin |
| Disclaimers | pending | 4 frentes (UI/CLI/reports/logs) |

## Hallazgo C1 (relevante)
`bortfeldProtonCurve` reescrito a la forma canónica Bortfeld 1997 (parabolic cylinder via Kummer). Mejora cualquier consumidor de curvas Bragg (incl. BioSimRunner/F4).

## Procedimiento (validado)
Agentes `isolation:worktree` + PASO 0 `git reset --hard integration/reconstruction`. Merge + gate (headless+Qt) + limpiar worktree. Para fixes de física: opus.

## Reglas de seguridad
- Merge a `main` solo al final de F6.
- No borrar `src/io` (DIFERIDO). No borrar `BioSimWidget` (conservado).
- Fase que rompe tests → revert inmediato.
