#include "services/export/ParquetExporter.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

namespace beamlab::services::export_ {

ExportResult ParquetExporter::exportData(
    const storage::IStorageBackend& storage,
    const analysis::AnalysisResult& result,
    const fs::path& outputPath,
    ExportProgressCallback onProgress)
{
    if (onProgress) onProgress(0.0f, "Preparing Parquet export (CSV interim)");

    // ── Determine output path ──────────────────────────────────────
    // The interim file gets a .parquet.csv suffix so users know it is
    // CSV-formatted but destined for Parquet conversion.
    auto csvPath = outputPath;
    if (fs::is_directory(outputPath)) {
        csvPath = outputPath / "samples.parquet.csv";
    } else {
        auto parent = outputPath.parent_path();
        auto stem   = outputPath.stem().string();
        csvPath = parent / (stem + ".parquet.csv");
    }

    // ── Log a reminder about true Parquet ──────────────────────────
    std::cout << "[ParquetExporter] WARNING: export is interim CSV. "
              << "Integrate Apache Arrow for true Parquet format.\n";

    // ── Open output file ───────────────────────────────────────────
    fs::create_directories(csvPath.parent_path());
    std::ofstream out(csvPath);
    if (!out.is_open()) {
        return ExportResult{
            false,
            "Cannot write " + csvPath.string(),
            csvPath, 0
        };
    }

    // ── Write CSV header ───────────────────────────────────────────
    // The header matches the future Parquet schema exactly.
    out << "# ParquetExporter interim CSV — convert to Parquet with:\n"
        << "#   python3 -c \"import pandas as pd; "
        << "pd.read_csv('" << csvPath.filename().string()
        << "').to_parquet('samples.parquet')\"\n"
        << "# Schema: trajectory_id:int64, step_index:int64, "
        << "x_cm:float64, y_cm:float64, z_cm:float64, "
        << "edep_MeV:float64, kinE_MeV:float64\n"
        << "trajectory_id,step_index,x_cm,y_cm,z_cm,edep_MeV,kinE_MeV\n";

    // ── Iterate batches ────────────────────────────────────────────
    uint64_t totalSamples = storage.totalSamples();
    uint64_t batchSize = 100000;
    uint64_t samplesWritten = 0;

    for (uint64_t offset = 0; offset < totalSamples; offset += batchSize) {
        auto batch = storage.readBatch(offset, batchSize);
        if (batch.data == nullptr || batch.count == 0) break;

        for (uint64_t i = 0; i < batch.count; ++i) {
            const auto& s = batch.data[i];
            out << s.trajectory_id.value() << ","
                << 0 << ","                          // step_index (placeholder)
                << (s.position_m.x * 100.0) << ","   // m → cm
                << (s.position_m.y * 100.0) << ","
                << (s.position_m.z * 100.0) << ","
                << s.edep_MeV << ","
                << s.kinE_MeV << "\n";
        }

        samplesWritten += batch.count;

        // ── Progress callback ──────────────────────────────────────
        if (onProgress) {
            float pct = (totalSamples > 0)
                ? static_cast<float>(samplesWritten)
                  / static_cast<float>(totalSamples)
                : 1.0f;
            onProgress(pct, "Writing Parquet interim CSV");
        }
    }

    out.close();

    if (onProgress) onProgress(1.0f, "Parquet export complete (CSV interim)");

    uint64_t bytesWritten = static_cast<uint64_t>(fs::file_size(csvPath));

    return ExportResult{true, std::nullopt, csvPath, bytesWritten};
}

} // namespace beamlab::services::export_
