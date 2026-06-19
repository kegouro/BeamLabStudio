#pragma once

#include "services/export/IExporter.h"

namespace beamlab::services::export_ {

/// Exports raw samples to Apache Parquet format.
///
/// ## Current status: functional interim (CSV fallback)
///
/// This exporter produces a `.parquet.csv` interim file with the same
/// schema that the final Parquet file will have.  The file can be
/// converted to true Parquet with:
///
///   python3 -c "
///   import pandas as pd
///   pd.read_csv('samples.parquet.csv').to_parquet('samples.parquet')
///   "
///
/// ## Future (when Apache Arrow is integrated)
///
/// Once `find_package(Arrow)` is added to CMake, replace the CSV
/// write loop with:
///
///   arrow::Int64Builder id_builder;
///   arrow::DoubleBuilder x_builder, y_builder, z_builder;
///   for (auto batch : storage.readBatch(offset, batchSize)) {
///       id_builder.Append(batch.trajectory_id);
///       x_builder.Append(batch.position_m.x);
///       ...
///   }
///   auto table = arrow::Table::Make(schema, {id_col, x_col, y_col, z_col, ...});
///   PARQUET_THROW_NOT_OK(parquet::arrow::WriteTable(*table, ...));
///
/// The public API (exportData signature, format string, extension list)
/// is final and will not change when Arrow is added.
class ParquetExporter final : public IExporter {
public:
    std::string name() const override { return "Apache Parquet"; }
    std::string format() const override { return "parquet"; }
    std::vector<std::string> fileExtensions() const override {
        return {".parquet", ".pq"};
    }

    ExportResult exportData(
        const storage::IStorageBackend& storage,
        const analysis::AnalysisResult& result,
        const std::filesystem::path& outputPath,
        ExportProgressCallback onProgress) override;
};

} // namespace beamlab::services::export_
