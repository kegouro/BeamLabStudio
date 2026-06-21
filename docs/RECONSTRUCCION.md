# Reconstrucción BeamLabStudio — Estado

> Rama: `integration/reconstruction` · HEAD `3305424` · Fecha: 2026-06-21
> Plan maestro: [`PLAN_MAESTRO.md`](PLAN_MAESTRO.md) · Tracking vivo: `.claude/PHASE_STATE.md`

## Resumen

Reconstrucción por fases de BeamLabStudio: reparación de lo roto (simulador biológico, gradientes de energía), rigor científico validado contra NIST/ICRU, limpieza de deuda, y expansión con features. Cada fase se ejecutó en un worktree aislado por un subagente, se integró con gate (build headless + Qt + tests) y se mergeó a `integration/reconstruction`. **Nunca se ha tocado `main`** (se hará solo al cierre de F6).

## Estado actual

| | |
|---|---|
| Build headless | ✅ 0 errores |
| Build Qt (`-DBEAMLAB_ENABLE_QT_UI=ON -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt`) | ✅ app bundle `BeamLabStudio.app` |
| Tests (`ctest -L unit`) | ✅ **223/223** (2 skipped = plugins `.so`) |
| Warnings | 0 propios · `-Werror` per-target en core/domain |
| CI | `.github/workflows/ci.yml` (build + ctest + regresión física) |
| Performance | `tests/performance/baseline.json` |

## Fases completadas (F0–F5)

| Fase | Qué se hizo |
|------|-------------|
| **F0** | Higiene: −19 GB de basura versionada, fix de build (símbolo duplicado `StoppingPowerEngine`, `beamlab_ui`↔`beamlab_domain`, label `unit`), `CLAUDE.md` actualizado |
| **F1a** | Física del dominio: I-value ICRU-90 + validación NIST PSTAR **real** (no autorreferencial). Dominio a **0.48 % vs NIST** |
| **F1b** | Física biosim: `StoppingPowerEngine` ya a 0.95 % (no se tocó). Corregidos 2 bugs reales de straggling: varianza de Bohr con β² y sesgo del sampler de Landau |
| **F2** | Pipeline: tabla de carga PDG-2022, validación unificada batch/streaming, round-trip IEEE-754 a 17 dígitos (verificado por `memcmp`), goldens TC01–06 |
| **F3** | Gradientes de energía: match por índices (no por bits), registro en nav rail, `EnergyColorMapper` unificado (−2 colormaps duplicados), selector de paleta |
| **F4** | Simulador biológico: ruta MVP end-to-end (`BioSimView → BioSimPresenter → BioSimRunner`) con física real; `BioSimWidget` legacy desactivado |
| **F5** | Limpieza: `pipeline_` muerto, 0 warnings + `-Werror`, **+93 tests reconectados** (114→207), `baseline.json`, `ci.yml` |

## Features F6

| Feature | Estado | Detalle |
|---------|--------|---------|
| **C1 SOBP** | ✅ done | Panel Spread-Out Bragg Peak en el tab BioSim, interactivo, pesos por back-substitution + refinamiento NNSL ligero. Meseta plana 1.4–2.4 %. **Bonus:** reescritura de `bortfeldProtonCurve` a la forma canónica (Bortfeld 1997, parabolic cylinder functions) — antes no tenía Bragg peak |
| **C4 Panel validación NIST** | ✅ done | Tab "Validación": dominio y biosim vs NIST lado a lado. Protón/agua validado (0.48 % / 0.95 %). Otras combinaciones → comparación inter-motor marcada "sin referencia NIST publicada" (honesto, no inventa datos) |
| C2 DVH | ⏳ pendiente | histograma dosis-volumen |
| C3 LET/RBE | ⏳ pendiente | colormap, reusa `EnergyScaleConverter` |
| P1 Export científico | ⏳ pendiente | Parquet/ROOT + DICOM-RT |
| P2 Reporte PDF | ⏳ pendiente | — |
| P3 Consola Python/plugins | ⏳ pendiente | `pybeamlab` / `IPlugin` |
| Disclaimers (4 frentes) | ⏳ pendiente | UI / CLI / reports / logs |

## Hallazgos científicos clave

1. **El "+25 % vs NIST" del diagnóstico no era del dominio** — el `SimulationEngine` ya estaba correcto (0.48 %). El problema vivía en el `StoppingPowerEngine` legacy de biosim, y eran bugs de straggling, no de Bethe-Bloch.
2. **`bortfeldProtonCurve` no producía un Bragg peak** (ley de potencia errónea, monótona decreciente). Reescrito a la forma cerrada de Bortfeld 1997. Mejora a todo consumidor de curvas Bragg (SOBP, BioSimRunner).
3. **No hay datos NIST PSTAR descargables para bone/muscle** vía web; calcularlos sería validación circular. Por eso C4 sólo declara "validado vs NIST" para protón/agua y usa comparación inter-motor honesta para el resto.

## ⚠️ Límite no-clínico (pendiente de formalizar en F6)

Rigor validado vs NIST/ICRU **no equivale** a certificación clínica (IEC 62083 / FDA / CE). Los disclaimers en 4 frentes (UI/CLI/reports/logs) están planificados pero **aún no implementados**.

## Diferido (backlog)

Migración `src/io → src/services` + borrar `src/io` + `ApplicationBootstrap → AnalysisOrchestrator`: alto riesgo, el pipeline activo funciona. Fase dedicada futura.

## Cómo verificar

```bash
# Build + tests
cmake -B build && cmake --build build -j$(sysctl -n hw.ncpu)
ctest --test-dir build -L unit --output-on-failure        # 223/223

# UI (biosim, gradientes, SOBP, validación NIST)
./launch_beamlabstudio.sh        # o: open build-qt/src/ui/BeamLabStudio.app
```
En la UI: tab **BioSim** (cargar CSV → Run → SOBP), tab **Trajectories 3D** (Energy gradient + paletas), tab **Validación** (dominio/biosim vs NIST).
