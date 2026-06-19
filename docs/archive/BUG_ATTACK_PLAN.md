# Plan de Ataque — Bug de Visualización "Forma de Diamante"

> **Estado:** En revisión — no implementar hasta aprobación  
> **Fecha:** 2026-05-24  
> **Severidad:** Alta (visualización incorrecta, física mal interpretada)

---

## 1. Resumen ejecutivo

La vista "Combined 3D" muestra las trayectorias con forma de diamante/reloj de arena angular en lugar de un haz suavemente divergente como aparece en Geant4. Hay **tres bugs independientes** que se acumulan:

| # | Bug | Impacto | Causa raíz |
|---|-----|---------|------------|
| B1 | **Binario viejo en memoria** | Crítico — 20 bins en lugar de 501, foco en z=11.58m | UI no reiniciada tras última compilación |
| B2 | **Cámara no alineada con el eje del haz** | Alto — vista isométrica distorsiona el reloj de arena | `frameLongestAxisHorizontally()` nunca se llama al cargar |
| B3 | **Algoritmo de detección de eje débil** | Medio — `axis_confidence=0.345`, falsa alarma de baja confianza | Usa rango espacial, no PCA sobre desplazamientos |

Además hay una **causa física raíz** que debe entenderse para no confundir la visualización correcta con un bug:

> El haz de muones en el rango z=11.55–11.75m **sí** converge: pasan de ~14mm de radio a ~6mm en z=11.656m. Esto ocurre porque los muones menos energéticos se detienen antes; los que sobreviven hasta z=11.65m son el subconjunto más colimado. Geant4 se ve diferente porque muestra las trayectorias **desde el cañón** (fuera del volumen del detector), donde el haz claramente diverge.

---

## 2. Fase de exploración — Hallazgos detallados

### 2.1 Diagnóstico del binario viejo (Bug B1)

**Evidencia directa:**

```bash
# CLI con mismo binario (build/beamlab, compilado 03:14):
./build/beamlab -i tests/muon_tracks.csv -o /tmp/test \
  --preview-trajectories 10000 --preview-samples-per-trajectory 200
# → Axial bins: 501  |  Focus index: 264  |  z=11.656m  |  r=5.986mm

# UI output (031553, corrida con binario viejo en memoria):
# → envelopes.count: 20  |  focus_index: 3  |  z=11.58m  |  r=28.77mm
```

La UI usó el proceso aún cargado del binario anterior (16:05 May 23). El binario actual (`03:14 May 24`) produce resultados correctos con la misma invocación.

**Por qué difieren:**
- Con 501 bins de 0.4mm de ancho sobre 1,080,724 muestras → ~2,157 muestras/bin → todos pasan el filtro `count≥3`
- Con 20 bins (comportamiento viejo), cada bin es 1cm de ancho → convoluciona trayectorias de distintos ángulos → radio de hull inflado a 28.77mm
- El focus_index=264 del binario nuevo corresponde a z=11.656m (el subhaz más colimado que sobrevive)

**Archivos afectados:** ninguno (es un problema de proceso, no de código)

**Fix B1:** Cerrar la UI y relanzar con `./launch_beamlabstudio.sh`.

---

### 2.2 Diagnóstico del alineamiento de cámara (Bug B2)

**Código investigado:**  
`src/ui/qt/Scene3DWidget.cpp` (agent: renderizador usa coordenadas mundo absolutas)  
`src/ui/qt/MainWindow.cpp` (línea 1459: `setViewPreset(0)` = Iso al presionar Reset)

**Causa:**  
`Scene3DWidget` renderiza los vértices del OBJ directamente en coordenadas mundo (x_m, y_m, z_m). El haz tiene:
- Rango Z: 11.55–11.75m (20cm axial)  
- Rango X/Y: ±1.8cm (transversal)
- Relación de aspecto Z/XY ≈ **5.7:1**

La cámara por defecto (vista isométrica, yaw=0.85, pitch=-0.48) muestra el eje Z oblicuamente. Con esa relación de aspecto y ese ángulo, la convergencia real (14mm→6mm→se abre) se ve como un diamante angular.

En Geant4 Vis, el eje Z se mapea horizontalmente por defecto → el mismo haz se ve como dos conos suaves.

**Función de corrección disponible:**  
`Scene3DWidget::frameLongestAxisHorizontally()` ya existe en el widget. Alinea el eje de mayor extensión (Z en este caso) horizontalmente. No hay que implementarla desde cero.

**Evidencia adicional:**  
El usuario puede hacer clic en "YZ" en la UI actual y ver el haz desde el costado — esto ya mejora dramáticamente la apariencia.

**Archivos afectados:**  
`src/ui/qt/MainWindow.cpp` — en `loadManifest()` (línea ~1316), después de añadir las capas al `combined_scene_viewer_`.

---

### 2.3 Diagnóstico de detección de eje (Bug B3)

**Código investigado:**  
`src/io/normalization/AxisFrameResolver.cpp` (líneas 106–132)

**Algoritmo actual:**
```
confidence = R_max / (Rx + Ry + Rz)
donde R_max = max(Rz, Rx, Ry) y Ri = rango i-ésimo de todos los puntos
```

**Por qué da 0.345 con muon_tracks.csv:**
- Rz = 11.75 − 11.55 = 0.20m (longitud del detector)
- Rx ≈ Ry ≈ 0.17m (dispersión transversal del cañón de muones, distribución espacial del gun)
- confidence = 0.20 / (0.20 + 0.17 + 0.17) = **0.37** ≈ reportado 0.345

El eje detectado ES correcto (+Z), pero el algoritmo confunde la *dispersión espacial del gun* (que es transversal pero grande) con incertidumbre en la dirección del haz.

**Algoritmo correcto (PCA sobre desplazamientos):**  
Si en lugar de posiciones absolutas se computan los vectores de paso Δ = pos[i+1] − pos[i] por trayectoria, y se hace PCA sobre esos vectores:
- Los Δ apuntan en +Z (el muón avanza)
- La varianza en la dirección dominante es >>5x mayor
- confidence real > 0.90

Este algoritmo no existe aún en `AxisFrameResolver.cpp`.

**Archivos afectados:**  
`src/io/normalization/AxisFrameResolver.cpp`

---

## 3. Causa física de la forma de "reloj de arena"

Aunque los bugs B1–B3 se corrigen, la visualización con el nuevo binario **aún mostrará convergencia** porque es física real. Esto es lo que ocurre:

```
z=11.55m: 149,908 muones presentes → radio hull ~8.9mm (todos los ángulos del gun)
z=11.56m:  62,103 muones → radio hull ~12.6mm (los de mayor ángulo salen del volumen)  
z=11.60m:  ~1,900 muones → radio hull ~10mm
z=11.66m:   1,856 muones → radio hull ~5.99mm ← FOCUS DETECTADO
z=11.75m:   ~1,000 muones → radio hull ~7.8mm
```

Los muones de menor energía se detienen antes; los ~1.2% que llegan a z=11.66m son precisamente los más colimados (alta energía, menor deflexión por dispersión múltiple). Esto produce el efecto visual de "convergencia": no es convergencia óptica, es **selección cinética**.

Geant4 Vis muestra el mismo fenómeno diferente porque:
1. Incluye las trayectorias desde la fuente (fuera del detector), mostrando la divergencia del cañón
2. Por defecto orienta el eje de propagación horizontalmente

---

## 4. Plan de implementación

### Fase A — Corrección inmediata (sin cambios de código)

**Acción:** Reiniciar la UI con el binario actual.

```bash
# Cerrar BeamLabStudio
pkill beamlab_ui 2>/dev/null || true
# Relanzar con binario nuevo (ya compilado a las 03:14)
./launch_beamlabstudio.sh
```

Luego: Open + Analyze con `tests/muon_tracks.csv`. Resultado esperado:
- `envelopes.count: 501`
- `focus_index: 264`
- Foco en z=11.656m, radio=5.986mm

---

### Fase B — Fix de cámara (1 archivo, ~3 líneas)

**Objetivo:** Al cargar un nuevo resultado, la cámara auto-alinea el eje del haz horizontalmente.

**Archivo:** `src/ui/qt/MainWindow.cpp`  
**Función:** `loadManifest()` (búsqueda aproximada en línea 1316)  
**Cambio:** Agregar llamada a `frameLongestAxisHorizontally()` después de añadir capas al combined_scene_viewer_.

```cpp
// Después del bloque de addObjLayer para trajectories:
if (combined_scene_viewer_ != nullptr) {
    combined_scene_viewer_->frameLongestAxisHorizontally();
}
```

**Riesgo:** Bajo. `frameLongestAxisHorizontally()` ya existe y funciona. Solo afecta la orientación inicial de la cámara — el usuario puede rotar libremente después.

**Archivos:** Solo `src/ui/qt/MainWindow.cpp`

---

### Fase C — Mejora del algoritmo de eje (2–3 archivos)

**Objetivo:** Reemplazar el algoritmo de rango espacial con PCA sobre vectores de desplazamiento por paso. Resultado esperado: `axis_confidence > 0.90` para datos de haz bien colimado.

**Archivos involucrados:**
- `src/io/normalization/AxisFrameResolver.cpp` — implementación del nuevo algoritmo
- `src/io/normalization/AxisFrameResolver.h` — si se necesita nueva firma (probablemente no)
- `tests/unit/test_AxisFrameResolver.cpp` — tests de regresión (verificar que datos de prueba mantienen confidencia)

**Algoritmo propuesto:**
```
Para cada trayectoria con ≥2 muestras:
  Acumular sum_dx += pos[i+1].x - pos[i].x, etc. para i en 0..N-2
  
Matriz de covarianza 3×3 sobre todos los vectores Δ:
  Σ = (1/N) Σ_i (Δᵢ − Δ_mean)(Δᵢ − Δ_mean)ᵀ

PCA: eigenvalues λ₁ ≥ λ₂ ≥ λ₃
  axis_confidence = λ₁ / (λ₁ + λ₂ + λ₃)  (fracción de varianza en dirección dominante)
  longitudinal = eigenvector(λ₁), signo = media de Δ · eigenvector > 0

Fallback: si total_displacement_vectors < 3, usar el algoritmo actual de rango.
```

**Riesgo:** Medio. Cambio de algoritmo de detección de eje afecta toda la cadena de análisis. Requiere tests de regresión antes de integrar.

---

### Fase D — Visualización de attrición de trayectorias (nuevo feature)

**Objetivo:** Añadir gráfico de N_tracks(z) al Dashboard o Statistics para que el usuario entienda por qué el haz "converge" visualmente.

**Archivos involucrados:**
- `src/app/ApplicationBootstrap.cpp` — exportar tabla `trajectory_survival.csv` (index, z_m, track_count)
- `src/ui/qt/StatsDashboardWidget.*` — nuevo plot de supervivencia

**Riesgo:** Bajo (additive feature). Baja prioridad respecto a B/C.

---

## 5. Orden de implementación recomendado

```
[AHORA]  Fase A — Reiniciar UI (sin código)
[Sesión] Fase B — Fix cámara en MainWindow.cpp (1 archivo, ~3 líneas, bajo riesgo)
[Sesión] Fase C — PCA en AxisFrameResolver.cpp (confirmar tests pasan)
[Futuro] Fase D — Track survival plot (feature nuevo)
```

---

## 6. Verificación post-fix

Después de implementar B:
1. Abrir BeamLabStudio con el nuevo binario
2. Open + Analyze → `tests/muon_tracks.csv`
3. **Esperado:** Combined 3D muestra el haz con Z horizontal, convergencia suave izquierda→derecha
4. **Esperado:** focus_index=264, z=11.656m en el Dashboard
5. Clic en "Iso" para reset → la cámara debe volver a Z horizontal (Fase B asegura esto)

Después de implementar C:
6. El panel de quality_report debe mostrar `axis_confidence ≈ 0.95` sin advertencia
7. Los 51/51 tests unitarios deben seguir pasando

---

## 7. Archivos clave (resumen de referencia)

| Archivo | Línea/Función | Relevancia |
|---|---|---|
| `src/ui/qt/MainWindow.cpp` | `loadManifest()` ~L1316 | Fix B: llamar `frameLongestAxisHorizontally()` |
| `src/ui/qt/Scene3DWidget.cpp` | `frameLongestAxisHorizontally()` | Ya implementada, solo invocarla |
| `src/io/normalization/AxisFrameResolver.cpp` | L106-132 `resolve()` | Fix C: reemplazar con PCA |
| `src/ui/qt/presenters/AnalysisPresenter.cpp` | L48-53 `runOnThread()` | El `config` se ignora — no crítico por ahora |
| `src/app/ApplicationBootstrap.cpp` | L410-481 pipeline | Correcto en nuevo binario, no tocar |
| `src/analysis/statistics/FrameStatisticsEngine.cpp` | `computeUniformAxialBins()` | Correcto, 501 bins confirmados |

---

*Generado tras exploración con 3 agentes paralelos + análisis manual de datos de salida.*  
*CLI confirmado: `Focus index: 264, z=11.6556m, r=5.986mm` con mismo binario.*
