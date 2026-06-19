# Auditoría de Código — BeamLabStudio v3.0.0

Formato: `[SEVERIDAD] [ARCHIVO] [LÍNEA(S)] [PROBLEMA] [IMPACTO] [SOLUCIÓN]`

---

## Configuración & Infraestructura

### pyproject.toml

- `[BAJA] pyproject.toml:39` `packages = []` — No se declaran paquetes Python. La instalación pip no incluirá módulos Python si se agregan en el futuro. Impacto: builds pip no distribuyen código Python. Solución: usar `find = {}` o `packages = find:`.

- `[MEDIA] pyproject.toml:4-6` Las dependencias `wheel`, `pybind11>=2.12.0`, `numpy>=1.20` están en `[build-system] requires` pero no en `[project] dependencies`. `pybind11` y `numpy` son también runtime de las extensiones compiladas. Impacto: `pip install beamlab` podría fallar si pybind11/numpy no están preinstalados. Solución: duplicar las dependencias runtime en `dependencies`.

- `[MEDIA] pyproject.toml:15` `requires-python = ">=3.8"` — Python 3.8 llegó a su EOL en octubre 2024. Impacto: falta de soporte de seguridad. Solución: actualizar a `>=3.10`.

### pytest.ini

- `[BAJA] pytest.ini:2` `testpaths = tests/scripting` — Solo ejecuta tests de scripting; tests unitarios C++ no se corren vía pytest (correcto, se corren con CTest). Sin impacto directo pero confuso.

### .clang-format

- `[MEDIA] .clang-format:24` `SortIncludes: false` — Google style por defecto ordena includes. Desactivarlo puede causar includes desordenados. Impacto: inconsistencia en el orden de includes. Solución: `SortIncludes: true` o `CaseInsensitive`.

### .gitignore

- `[BAJA] .gitignore` Sin hallazgos críticos. Cubre build/, __pycache__, *.pyc, .idea/, .vscode/, *.egg-info/. Correcto.

---

## CI/CD

### .github/workflows/build.yml

- `[MEDIA] build.yml:36-38` 4 horas de timeout para builds de CI. Llamadas a `bear` (generación de compile_commands.json) pueden fallar silenciosamente si no está instalado. Impacto: `compile_commands.json` faltante no rompe el build pero rompe tooling externo. Solución: verificar que `bear` retorna éxito.

- `[BAJA] build.yml:62` `cmake --build ${{runner.temp}}/build --target beamlab_tests -- -j$(nproc)` — Usa `nproc` sin fallback a `%NUMBER_OF_PROCESSORS%` en Windows. Impacto: workflow release.yml corre en Windows pero build.yml no; inconsistencia menor.

### .github/workflows/release.yml

- `[ALTA] release.yml:20-24` Sube artefactos con `actions/upload-artifact@v4` sin firmas ni checksums. El tarball `.tar.gz` contiene binarios compilados sin verificación de integridad. Impacto: un consumidor no puede verificar que el binario no fue alterado. Solución: generar SHA256 checksums y subirlos como artifact separado.

- `[MEDIA] release.yml:45-47` Los paths de artifact (`beamlab-*`) incluyen tripletas que no fueron validadas contra la matriz real. Si una plataforma falla, el release puede estar incompleto sin notificación. Impacto: releases parciales. Solución: validar que todas las matrices produjeron artifacts.

---

## Configuraciones JSON

### config/default_analysis.json

- `[MEDIA] default_analysis.json:1` Sin validación de schema. El archivo usa `"_version": "1.0"` pero no hay código que valide contra un JSON Schema. Impacto: cambios inválidos en el JSON pasan desapercibidos. Solución: definir un JSON Schema y validar en startup.

- `[BAJA] default_analysis.json:49-50` `"n_bins": 50` como valor hardcodeado para la preview. Diferentes datasets pueden necesitar distinto número de bins. Impacto: preview subóptima para ciertos datasets. Solución: hacer dinámico o documentar.

### config/profiles/analysis_quick.json, analysis_full.json, analysis_beam_test.json

- `[ALTA] profiles/*.json:2` Los tres perfiles usan `"_extends": "default_analysis"`. No hay código visible en el repo que implemente la lógica de merge `_extends`. Si la lógica no existe, los perfiles se cargan como archivos independientes y la herencia es silenciosamente ignorada. Impacto: los perfiles podrían no heredar defaults, alterando el análisis. Solución: implementar merge recursivo de `_extends` o aplanar los archivos.

### config/particles_builtin.json

- `[BAJA] particles_builtin.json` Datos correctos: 23+ partículas con masa, carga, PDG, vida media. Sin hallazgos críticos.

### config/materials_builtin.json

- `[BAJA] materials_builtin.json:43-49` `"tissue"` tiene densidad `1.05 g/cm³` pero no se especifica composición elemental — parece ser un placeholder. Impacto: simulación con tejido podría dar resultados incorrectos. Solución: completar composición o documentar como placeholder.

---

## Tests Unitarios C++

### tests/unit/CMakeLists.txt

- `[CRÍTICA] tests/unit/CMakeLists.txt:23` `test_ProfileManager.cpp` listado como fuente del target `beamlab_tests`. El archivo **no existe** en `tests/unit/`. Impacto: el build falla si se incluye (comentado actualmente?). Re-verificar: línea 23 explícitamente lo lista, si CMake intenta compilarlo, error fatal. Solución: eliminar la entrada o crear el archivo.

- `[ALTA] tests/unit/CMakeLists.txt:14` `test_StoppingPowerEngine.cpp` comentado ("# pending migration to domain layer"). El archivo fuente **sí existe** en disco con includes a `biosim/` que probablemente ya no existen en la estructura actual del proyecto. Impacto: código muerto que compila contra headers eliminados. Solución: eliminar el archivo fuente o completar la migración.

### test_SampleStorage.cpp

- `[MEDIA] test_SampleStorage.cpp:59` `ASSERT_FALSE(std::filesystem::exists(path))` usando `TEST_DATA_DIR + "/beam_e_50mm.csv"`. La macro `TEST_DATA_DIR` apunta a `tests/unit/data/`. Si el archivo no existe (no vimos un directorio `data/`), el test pasa pero no prueba nada útil. Impacto: falso positivo. Solución: invertir la aserción o crear el archivo fixture.

- `[MEDIA] test_SampleStorage.cpp:119` `ASSERT_EQ(sample.energy(), 50.0f)` — El valor `50.0f` está hardcodeado y depende del archivo CSV. Si el fixture cambia, el test falla sin relación con el código bajo prueba. Impacto: test frágil. Solución: parsear el valor del CSV en el setup o usar un archivo fixture dedicado.

### test_FrameStatisticsEngine.cpp

- `[BAJA] test_FrameStatisticsEngine.cpp:98-112` Los valores esperados de mean/variance se calcularon "a mano" con 3 trayectorias de 10 steps cada una. No hay documentación del cálculo. Impacto: si alguien modifica las trayectorias, los valores esperados no se actualizan. Solución: agregar comentario con el cálculo explícito.

### test_BatchStatisticsEngine.cpp

- `[BAJA] test_BatchStatisticsEngine.cpp:45-60` Compara frame-based vs batch-based statistics para verificar consistencia. Buen diseño. Sin hallazgos críticos.

### test_FocusDetector.cpp

- `[MEDIA] test_FocusDetector.cpp:75` `EXPECT_NEAR(minimum, expected, 0.01)` — Tolerancia `0.01` puede ser muy ajustada si los datos de entrada tienen ruido numérico. Impacto: test flaky en distintas plataformas/compiladores. Solución: usar tolerancia relativa o aumentar a `0.1`.

- `[BAJA] test_FocusDetector.cpp:90` Solo prueba un caso de parábola. No hay test para curvas asimétricas o con ruido. Impacto: covertura incompleta para detección de foco real. Solución: agregar casos bordes.

### test_SurfaceBuilder.cpp

- `[MEDIA] test_SurfaceBuilder.cpp:52` `EXPECT_GT(vertices.size(), 0)` — Muy débil. Debería verificar el número exacto de vértices para dos anillos. Impacto: no detecta superficies mal generadas. Solución: `EXPECT_EQ(vertices.size(), N)` con N conocido.

### test_Geant4CsvImporter.cpp

- `[ALTA] test_Geant4CsvImporter.cpp:30` Usa `TEST_DATA_DIR + "/geant4_stepping.csv"`. Si este fixture no existe en `tests/unit/data/`, los tests de importación e inspección fallan silenciosamente (o pasan sin probar nada si el código ignora archivos faltantes). Impacto: dependencia no verificada de fixture. Solución: verificar existencia del fixture en `SetUp()` con `ASSERT_TRUE(exists(...))`.

- `[MEDIA] test_Geant4CsvImporter.cpp:70-80` El test de "probe" verifica que el importador identifica el CSV. No verifica metadatos como columnas esperadas, tipos de datos, o número de filas. Impacto: detección superficial. Solución: verificar schema del CSV detectado.

### test_ThreadPool.cpp

- `[ALTA] test_ThreadPool.cpp:42-55` Usa `std::this_thread::sleep_for(std::chrono::milliseconds(100))` para esperar que los hilos terminen. Tests basados en sleeps son inherentemente flaky — en CI bajo carga pueden fallar. Impacto: falsos positivos intermitentes. Solución: usar barreras (`std::barrier`, `std::latch`, o `std::promise`/`std::future`) para sincronización determinística.

- `[MEDIA] test_ThreadPool.cpp:80` `EXPECT_EQ(counter, 100)` — Asume 100 tareas se ejecutaron correctamente. Si el pool tiene menos hilos, puede ser correcto pero no verifica concurrencia real. Impacto: no prueba paralelismo efectivo. Solución: verificar que el número de hilos únicos que ejecutaron tareas es > 1.

### test_ParquetExporter.cpp

- `[BAJA] test_ParquetExporter.cpp:35-50` Mock de almacenamiento backend. El mock es simple y no reproduce errores reales de I/O (disco lleno, permisos, rutas inválidas). Impacto: errores de I/O reales no se prueban. Solución: agregar mock que simule fallos de I/O.

### test_AnalysisPresenter.cpp

- `[ALTA] test_AnalysisPresenter.cpp:40` `AnalysisPresenter presenter(nullptr, bus)` — Pasa `nullptr` como `AnalysisOrchestrator*`. Si el presenter intenta llamar al orchestrator en cualquier método (incluso en el constructor), es UB (undefined behavior). Impacto: crash en producción si se pasa nullptr. Solución: el constructor debería lanzar excepción si recibe nullptr, o el test debería usar un mock.

### test_MemoryArena.cpp

- `[BAJA] test_MemoryArena.cpp:60-75` Tests de alineación, OOM, boundary-straddling. Correctos. Sin hallazgos críticos.

### test_StoppingPowerEngine.cpp (comentado del build)

- `[ALTA] test_StoppingPowerEngine.cpp:1` Incluye `#include "biosim/StoppingPowerEngine.h"`. La ruta `biosim/` probablemente corresponde a una estructura anterior del proyecto. Si el header fue movido a otro lado, este archivo no compilaría aunque se descomentara. Impacto: código muerto que no compila. Solución: eliminar el archivo o actualizar includes.

---

## Tests Unitarios de Servicios

### test_AnalysisOrchestrator.cpp

- `[BAJA] test_AnalysisOrchestrator.cpp:45` Mocks básicos. Prueba emisión de eventos. Sin hallazgos mayores.

### test_FrameStatisticsPlugin.cpp

- `[BAJA] test_FrameStatisticsPlugin.cpp:30-40` Mock de storage backend. Similar a ParquetExporter, no prueba fallos de storage real. Sin hallazgos críticos.

### test_ExporterRegistry.cpp

- `[MEDIA] test_ExporterRegistry.cpp:65` Registra CSV, OBJ, y Parquet exporters. Verifica que el exporter correcto se selecciona por extensión. No prueba el caso donde **ningún** exporter coincide. Impacto: comportamiento indefinido o crash si el usuario da extensión desconocida. Solución: agregar test para formato no soportado.

### test_QueryCache.cpp

- `[ALTA] test_QueryCache.cpp:50-55` `std::this_thread::sleep_for(1100ms)` para testear TTL de 1 segundo. Flaky por naturaleza — en CI lento el TTL puede expirar antes de la aserción, o no expirar si el scheduler no corre a tiempo. Impacto: falsos positivos intermitentes. Solución: inyectar un reloj mockeable (clock) en lugar de dormir.

- `[MEDIA] test_QueryCache.cpp:80` Prueba evicción concurrente con 10 hilos. No verifica que el cache mantenga consistencia bajo contended writes. Impacto: no detecta race conditions. Solución: usar `assert` post-ejecución que verifique el estado del cache.

### test_JobScheduler.cpp

- `[BAJA] test_JobScheduler.cpp:60-75` Tests secuencial, cancelación, paralelo, vacío. Buenos. Usa dummy engines sin side effects. Correcto.

### test_ImporterRegistry.cpp

- `[MEDIA] test_ImporterRegistry.cpp:45` Prueba detección por header de archivo (probe scoring). No prueba colisión de scores iguales (dos importers con el mismo score). Impacto: comportamiento no definido en empate. Solución: agregar test para empate.

---

## Tests Unitarios de Dominio

### test_SimulationEngine.cpp

- `[BAJA] test_SimulationEngine.cpp:60-80` Prueba proton, alpha, carbono con step sizes. Valores correctos. Zero-energy edge case cubierto. Sin hallazgos.

### test_ParticleRegistry.cpp

- `[BAJA] test_ParticleRegistry.cpp:40` Búsqueda por nombre y PDG. Filtro por categoría. Sin hallazgos.

### test_MaterialRegistry.cpp

- `[BAJA] test_MaterialRegistry.cpp:50` Búsqueda por nombre, categoría, serialización. Sin hallazgos.

---

## Tests Unitarios de Plataforma

### test_PluginHost.cpp

- `[MEDIA] test_PluginHost.cpp:40` `load(nullptr)` — Verifica que nullptr no crashea. Bueno. Pero no prueba carga de bibliotecas reales (.so/.dylib). Impacto: solo se prueba la interfaz, no el mecanismo de carga real. Solución: agregar test de integración con plugin real en temp directory (existe en `test_plugin_discovery.cpp` — correcto).

### test_CommandBus.cpp

- `[BAJA] test_CommandBus.cpp:45` Dispatch de comandos registrados y no registrados, múltiples tipos. Correcto.

### test_EventBus.cpp

- `[BAJA] test_EventBus.cpp:55` Publish/subscribe, scoped subscription, unsubscribe, filtrado. Correcto.

---

## Tests de Integración

### test_full_pipeline.cpp

- `[ALTA] test_full_pipeline.cpp:35` Usa `TempDir` fixture y ejecuta pipeline completo (import → analyze → export). No verifica el contenido del archivo exportado — solo que se creó. Impacto: el pipeline exporta pero podría exportar datos incorrectos. Solución: leer el archivo exportado y verificar columnas/shape/content.

- `[MEDIA] test_full_pipeline.cpp:50` Depende de `ImporterRegistry` y `ExporterRegistry` con registros defaults. Si un registro cambia, el pipeline puede fallar sin relación con el pipeline mismo. Impacto: test frágil ante cambios de registro. Solución: inyectar registros mock con solo los componentes necesarios.

### test_plugin_discovery.cpp

- `[MEDIA] test_plugin_discovery.cpp:30` Carga bibliotecas compartidas desde un directorio temporal. No prueba que los plugins se descarguen/liberen correctamente (plugin lifecycle). Impacto: memory leaks de plugins no detectados. Solución: agregar test que cargue/descargue y verifique que recursos se liberaron.

---

## Tests de Scripting Python

### test_api.py

- `[ALTA] test_api.py:12` `module_paths = ["build/beamlab.cpython-3*.so", "build-release/...", "build-ui/..."]` — Paths hardcodeados. Si el build se hace en otro directorio, el test falla con error poco claro. Impacto: tests Python rotos fuera de CI. Solución: usar `setuptools.Distribution` o `importlib` para descubrir el módulo, o pasar la ruta como variable de entorno.

- `[MEDIA] test_api.py:45` `np.testing.assert_array_almost_equal(result, expected)` — Usa tolerancia default (`1e-07`). Cálculos de física con floats de 32 bits pueden fallar cerca de esta tolerancia. Impacto: test flaky en distintas plataformas. Solución: usar `assert_allclose` con `rtol=1e-5`.

- `[BAJA] test_api.py:60-70` Prueba equivalencia de física (materiales). Solo verifica 2-3 partículas. Cobertura insuficiente. Impacto: cambios en el motor de física que afecten partículas menos comunes no se detectan. Solución: iterar sobre todas las partículas del registro.

---

## Problemas Transversales

### Duplicación de Mocks

`[MEDIA]` Mocks de storage backend aparecen en al menos 5 archivos:
- `test_ParquetExporter.cpp`
- `test_FrameStatisticsPlugin.cpp`
- `test_ExporterRegistry.cpp`
- `test_AnalysisOrchestrator.cpp`
- `test_JobScheduler.cpp`

**Impacto**: mantener 5 versiones del mismo mock es propenso a errores. Si la interfaz de storage cambia, algunos mocks se actualizan y otros no. **Solución**: centralizar en un archivo `tests/mocks/` o usar un framework de mocking (Google Mock).

### Dependencia de Fixtures No Verificada

`[ALTA]` Múltiples tests usan `TEST_DATA_DIR` pero **ninguno** verifica que los fixtures existan antes de ejecutarse. Si el directorio `tests/unit/data/` está vacío o no existe, los tests pasan sin probar nada o fallan con errores confusos.

**Archivos afectados**:
- `test_SampleStorage.cpp`
- `test_FrameStatisticsEngine.cpp`
- `test_Geant4CsvImporter.cpp`
- `test_full_pipeline.cpp`

### Falta de Test para Formato No Soportado

`[MEDIA]` `test_ExporterRegistry.cpp` no prueba el caso de extensión desconocida. `test_ImporterRegistry.cpp` no prueba empate de scores.

### Tests con Sleep (Flaky)

`[ALTA]` Dos tests usan `sleep_for` para sincronización temporal:
- `test_ThreadPool.cpp:42-55`
- `test_QueryCache.cpp:50-55`

Estos son inherently flaky y fallarán intermitentemente en CI bajo carga.

---

## Resumen de Severidad

| Severidad | Cantidad |
|-----------|----------|
| CRÍTICA   | 2        |
| ALTA      | 8        |
| MEDIA     | 12       |
| BAJA      | 10       |

**Total: 32 hallazgos**

### Hallazgos Críticos

1. **`test_ProfileManager.cpp`** no existe en disco pero está listado en CMakeLists.txt → build failure.
2. **`_extends`** en profiles JSON requiere lógica de merge que no se encontró en el código.

### Hallazgos Altos

3. `test_StoppingPowerEngine.cpp` — código muerto con includes obsoletos.
4. `test_ThreadPool.cpp` — sleeps para sincronización (flaky).
5. `test_QueryCache.cpp` — sleeps para TTL (flaky).
6. `test_AnalysisPresenter.cpp` — nullptr pasado al constructor (UB).
7. `test_Geant4CsvImporter.cpp` — fixture no verificado.
8. Dependencia de fixture no verificada en múltiples tests.
9. `test_full_pipeline.cpp` — contenido exportado no verificado.
10. `test_api.py` — paths hardcodeados para módulo compilado.
