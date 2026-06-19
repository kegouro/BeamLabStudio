# Phase State — Reconstrucción BeamLabStudio

Rama integración: `integration/reconstruction`
Rama base: `feat/ui-redesign-macos`
Plan: `docs/PLAN_MAESTRO.md` (v2)

| Fase | Estado | Notas |
|------|--------|-------|
| F0  | done ✅ | higiene, build fix (B3/B4/B5), basura −19G, docs archivados, CLAUDE.md, gate verde (headless+Qt, 98/98) |
| F1a | pending | física básica Bethe-Bloch + validación NIST real (≤5% 50–250 MeV) |
| F1b | pending | física avanzada Sternheimer/shell/Vavilov/MCS (≤2% 10–300 MeV) |
| F2  | pending | pipeline datos & rigor (round-trip 17 dígitos, streaming≡batch) |
| F3  | pending | gradientes energía (match por índices, nav rail, EnergyColorMapper) |
| F4  | pending | biosim MVP (tras F1a) |
| F5  | pending | reestructura services + CI + baseline perf (tras F2) |
| F6  | pending | pulido + 7 features + docs |

## Features F6 elegidas
C1 SOBP · C2 DVH · C3 LET/RBE · C4 Panel validación NIST · P1 Export científico · P2 Reporte PDF · P3 Consola Python/plugins
Backlog: Comparador multi-haz.

## Worktrees activos
(ninguno aún)

## Reglas de seguridad
- Merge a `main` solo al final de F6.
- No borrar `src/io` sin F5 migrado+verificado. No borrar `BioSimWidget` sin F4 verificado.
- Fase que rompe tests en integración → revert inmediato.
