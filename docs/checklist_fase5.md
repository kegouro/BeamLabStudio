## Checklist Fase 5 — Plugin Ecosystem

### ImporterRegistry

- [ ] **Compila sin warnings**: `cmake --build build --target beamlab_services_import 2>&1 | grep -cE "error:|warning:"` retorna 0. Flags: `-Wall -Wextra -Wpedantic -Wconversion`
- [ ] **Geant4CsvImporter::probe()**: archivo con header `x_cm,y_cm,z_cm,edep_MeV,kinE_MeV,...` retorna score ≥ 0.95. Verificar con: `EXPECT_GE(probe(csv).value, 0.95)`
- [ ] **Geant4CsvImporter::probe() — match parcial**: archivo con solo 3 columnas canónicas retorna score ≥ 0.50 (no crash)
- [ ] **ComsolCsvImporter::probe()**: archivo con `%% Model:` en primeras 5 líneas retorna score ≥ 0.85. `EXPECT_GE(probe(comsol).value, 0.85)`
- [ ] **RootNativeImporter::probe()**: archivo con magic bytes `\x72\x6F\x6F\x74` ("root") retorna score = 1.0. Verificar con: `EXPECT_EQ(probe(root).value, 1.0)`
- [ ] **DelimitedTableImporter::probe()**: archivo CSV con 5 columnas numéricas retorna score ≥ 0.30 (fallback). `EXPECT_GE(probe(csv).value, 0.30)`
- [ ] **DetectImporter — best match**: mezcla de 4 importers registrados, archivo Geant4 → selecciona Geant4CsvImporter (score > todos los demás). Verificar con: `EXPECT_EQ(detect.bestMatch->name(), "Geant4 CSV")`
- [ ] **DetectImporter — alternatives**: importers con score > 0.3 que no ganan aparecen en `result.alternatives`
- [ ] **Import throw en archivo sin match**: `registry.import("unknown.xyz", storage, nullptr)` lanza `std::runtime_error`. Verificar con: `EXPECT_THROW(..., std::runtime_error)`
- [ ] **Archivo vacío**: detección retorna bestMatch=nullptr, score < 0.01. Sin crash.
- [ ] **Thread-safety**: `detectImporter()` usa `std::shared_lock` (lectura concurrente). Verificar con: ejecutar 4 detect simultáneos desde std::threads sin data race
- [ ] **Test completo**: `ctest -R test_ImporterRegistry --output-on-failure` pasa (12+ tests)

### ExporterRegistry

- [ ] **Compila sin warnings**: `cmake --build build --target beamlab_services_export 2>&1 | grep -cE "error:|warning:"` retorna 0
- [ ] **CsvExporter — header correcto**: archivo CSV generado contiene `frame_index,reference_value,reference_min,reference_max,point_count,centroid_u,centroid_v,sigma_u,sigma_v,r_rms,valid`. Verificar con: `head -1 export.csv`
- [ ] **CsvExporter — datos numéricos**: segunda línea del CSV contiene valores numéricos parseables (no strings). Verificar con: `awk -F',' 'NR==2{print $2+0}' export.csv` retorna número
- [ ] **ObjExporter — formato Wavefront**: archivo .obj comienza con `#` (comentario) y contiene líneas `v ` y `f `. Verificar con: `grep -c "^v " mesh.obj` ≥ 4
- [ ] **ParquetExporter — stub funcional**: genera archivo `.parquet.csv` con header `trajectory_id,step_index,x_cm,y_cm,z_cm,edep_MeV,kinE_MeV`. No crash. `result.success == true`
- [ ] **ParquetExporter — warning**: stdout contiene `WARNING: Parquet export is interim CSV`. Verificar con captura de `std::cout`
- [ ] **exportAll() — 3 formatos simultáneos**: `exportAll(storage, result, dir)` crea `csv/`, `obj/`, `parquet/` subdirectorios sin interbloqueos. Timestamp de archivos todos dentro de 1 segundo
- [ ] **exportAll() — fallo parcial**: si un formato falla, los otros continúan. `overallSuccess == false`, pero `results` contiene entry para cada formato
- [ ] **exportAll() — formatos específicos**: `exportAll(storage, result, dir, {"csv"})` exporta solo CSV (1 resultado, no 3)
- [ ] **exportSingle() — formato inexistente**: `exportSingle("nonexistent", ...)` lanza `std::out_of_range`
- [ ] **exportSingle() — progreso**: callback `onProgress` se invoca al menos al inicio (0%) y fin (100%)
- [ ] **Test completo**: `ctest -R test_ExporterRegistry --output-on-failure` pasa (12+ tests)

### PluginHost

- [ ] **Carga .so externo**: `host.load("/tmp/test_plugin.so")` retorna `optional<LoadedPlugin>` con `name` y `version` poblados. Verificar: `ASSERT_TRUE(loaded.has_value())`
- [ ] **Tres símbolos soportados**: `create_importer`, `create_exporter`, `create_plugin`. Verificar cargando un .so que exporte cada uno
- [ ] **`getPluginsOfType<IImporter>()`**: después de cargar un plugin .so con `create_importer`, aparece en el vector filtrado. Verificar: `dynamic_cast<IImporter*>(plugin) != nullptr`
- [ ] **`unload(name)`**: descarga el plugin. `host.loadedCount()` decrementa. Segunda llamada a `getPluginsOfType` no incluye el plugin descargado. `dlclose` se llama (verificar con `lsof` que el .so ya no está mappeado)
- [ ] **Builtin no se descarga**: `host.unload("DummyPlugin")` retorna `false`
- [ ] **Símbolo faltante → warning, no crash**: `.so` sin `create_*` símbolo → log a `std::cerr`, `load()` retorna `nullopt`, `loadedCount()` no cambia
- [ ] **scanDirectory()**: directorio con varios `.so` → carga todos. Verificar: `EXPECT_EQ(host.loadedCount(), N)`. Directorio vacío → `loadedCount()` sin cambios
- [ ] **`.so` corrupto → no crash**: archivo con bytes `0xDEADBEEF` → `dlopen` falla, load retorna `nullopt`. Sin segfault
- [ ] **RAII handle leak**: si `LoadedPlugin` se destruye sin `unload()`, el handle `dlopen` se cierra automáticamente (verificar con `SharedLibHandle` destructor)
- [ ] **initializeAll()**: plugin built-in recibe `initialize()` y `shutdown()` en el orden correcto. Verificar flags booleans
- [ ] **Test completo**: `ctest -R test_PluginHost --output-on-failure` pasa (12+ tests)

### Integración UI

- [ ] **`AnalysisPresenter::getAvailableImporterFilters()`**: retorna `QStringList` con primer elemento `"Todos los soportados (*.csv *.tsv *.dat *.parquet ...)"`. Verificar visualmente en UI
- [ ] **Filtro dinámico**: `QFileDialog::getOpenFileName()` usa el filtro generado. Extensiones `.myf` de plugins externos aparecen en el diálogo
- [ ] **Plugin externo en UI**: instalar `.so` en `~/.config/BeamLabStudio/plugins/importers/`, reiniciar BeamLabStudio, File → Open → el plugin aparece en la lista de formatos
- [ ] **`AnalysisPresenter::getAvailableExporterFormats()`**: retorna lista de formatos registrados (csv, obj, parquet). Verificar con `EXPECT_TRUE(list.contains("csv"))`
- [ ] **ApplicationBootstrap wiring**: `ApplicationBootstrap::run()` llama `initializePlugins()`, registra importers built-in, escanea directorios. Verificar log: `[bootstrap] Registered 3 built-in importers`
- [ ] **`ImporterRegistry` en ApplicationContext**: `m_context.services().resolve<ImporterRegistry>()` retorna la misma instancia usada por UI
- [ ] **`PluginHost` en ApplicationContext**: vive tanto como la aplicación. No se destruye antes que los registries

### Documentación

- [ ] **`docs/PLUGIN_DEVELOPMENT.md`** existe y contiene: overview, quick start, API reference, debugging, distribution
- [ ] **Template compila standalone**: `cd src/plugins/importers/template && cmake -B build && cmake --build build` completa sin errores
- [ ] **Template produce .so**: `build/libexample_importer.so` existe y exporta `create_importer`. Verificar: `nm -D build/libexample_importer.so | grep create_importer`
- [ ] **Quick start verificable**: un desarrollador siguiendo la guía completa los 8 pasos en <1 hora (verificar internamente con un junior)

### CI / Build

- [ ] **ctest sin regresiones**: `ctest --test-dir build --output-on-failure -j$(nproc)` reporta todos los tests como PASS. Comparar contra baseline: `ctest -N` cuenta ≥ 55 tests
- [ ] **Build Release en <150s**: `cmake -B build-release -DCMAKE_BUILD_TYPE=Release && cmake --build build-release -j$(nproc)` completa en <150s (target del blueprint)
- [ ] **Sin warnings nuevos**: comparar build log contra baseline de Fase 4. `cmake --build build 2>&1 | grep -E "warning:" | wc -l` no aumentó
- [ ] **`BEAMLAB_PLUGINS_DIR` configurable**: `cmake .. -DBEAMLAB_PLUGINS_DIR=/custom/path` setea la variable correctamente. Verificar en CMakeCache.txt
- [ ] **`BEAMLAB_BUILD_PLUGIN_TEMPLATES`**: `cmake .. -DBEAMLAB_BUILD_PLUGIN_TEMPLATES=ON` compila los templates sin errores
- [ ] **Cross-compilación**: `BEAMLAB_BUILD_PLUGIN_TEMPLATES=ON` con `BUILD_SHARED_LIBS=OFF` produce .so correctamente

### Comandos de verificación rápida

```bash
# 1. Compilación de servicios
cmake --build build --target beamlab_services_import 2>&1 | grep -cE "error:|warning:"
cmake --build build --target beamlab_services_export 2>&1 | grep -cE "error:|warning:"

# 2. Tests de Fase 5
ctest --test-dir build -R "ImporterRegistry|ExporterRegistry|PluginHost|ParquetExporter" --output-on-failure

# 3. Tests completos
ctest --test-dir build --output-on-failure -j$(nproc)

# 4. Template compila
cd src/plugins/importers/template && cmake -B build -DCMAKE_PREFIX_PATH=$BEAMLAB_DIR && cmake --build build

# 5. Símbolos del template
nm -D src/plugins/importers/template/build/libexample_importer.so | grep create_importer

# 6. Documentación
grep -c "## 3. Quick Start" docs/PLUGIN_DEVELOPMENT.md || echo "Quick Start section missing"

# 7. PluginHost carga y descarga
#   (verificado por test_PluginHost::LoadFromSharedLibrary)

# 8. Sin regresión de warnings
cmake --build build 2>&1 | grep -E "warning:" | grep -v "third_party" | wc -l
```
