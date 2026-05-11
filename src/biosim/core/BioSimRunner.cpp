#include "biosim/core/BioSimRunner.h"
#include "biosim/core/EnergyScaleConverter.h"
#include "biosim/physics/EnergyStraggling.h"
#include "biosim/physics/BraggPeakCalculator.h"
#include "core/foundation/NumericGuards.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <unordered_map>

namespace beamlab::biosim {

// ── CSV loader ────────────────────────────────────────────────────────────────
// Parses energy_step_profile.csv, header:
//   event_id, track_id, step_index, axial_m,
//   edep_MeV, edep_eV, kinE_MeV, dose_Gy,
//   x_m, y_m, z_m
bool BioSimRunner::loadCsv(const std::string& path,
                            std::vector<BioTrack>& tracks,
                            std::string& error_msg,
                            std::vector<std::string>& warnings_out) const
{
    std::ifstream f(path);
    if (!f) {
        error_msg = "Cannot open: " + path;
        return false;
    }

    // Build tracks in a map (keyed by (event_id, track_id)) so we never hold
    // pointers into a growing std::vector — that would dangle after reallocation.
    std::map<std::pair<int,int>, BioTrack> track_map;

    std::string line;
    bool header_done = false;
    int col_event{0}, col_track{1}, col_step{2}, col_axial{3};
    int col_edep_MeV{4}, col_kinE{6}, col_dose{7};
    int col_x{8}, col_y{9}, col_z{10};

    struct DropReasonStats { std::size_t count{0}; std::size_t first_line{0}; };
    std::unordered_map<std::string, DropReasonStats> drops;
    const auto recordDrop = [&](const char* reason, std::size_t line_no) {
        auto& s = drops[reason];
        if (s.count == 0) s.first_line = line_no;
        ++s.count;
    };

    std::size_t line_number = 0;
    while (std::getline(f, line)) {
        ++line_number;
        if (line.empty() || line[0] == '#') continue;

        if (!header_done) {
            std::istringstream hss(line);
            std::string tok;
            int col = 0;
            while (std::getline(hss, tok, ',')) {
                const std::string t = [&]{
                    std::string s = tok;
                    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                        [](unsigned char c){ return !std::isspace(c); }));
                    s.erase(std::find_if(s.rbegin(), s.rend(),
                        [](unsigned char c){ return !std::isspace(c); }).base(), s.end());
                    return s;
                }();
                if      (t == "event_id")   col_event    = col;
                else if (t == "track_id")   col_track    = col;
                else if (t == "step_index") col_step     = col;
                else if (t == "axial_m")    col_axial    = col;
                else if (t == "edep_MeV")   col_edep_MeV = col;
                else if (t == "kinE_MeV")   col_kinE     = col;
                else if (t == "dose_Gy")    col_dose     = col;
                else if (t == "x_m")        col_x        = col;
                else if (t == "y_m")        col_y        = col;
                else if (t == "z_m")        col_z        = col;
                ++col;
            }
            header_done = true;
            continue;
        }

        std::vector<std::string> cols;
        {
            std::istringstream ss(line);
            std::string tok;
            while (std::getline(ss, tok, ',')) cols.push_back(tok);
        }

        const int max_col = std::max({col_event, col_track, col_step, col_axial,
                                       col_edep_MeV, col_kinE, col_dose,
                                       col_x, col_y, col_z});
        if (static_cast<int>(cols.size()) <= max_col) {
            recordDrop("too few columns", line_number);
            continue;
        }

        const auto event_opt = beamlab::core::tryParseInteger(cols[static_cast<std::size_t>(col_event)]);
        const auto track_opt = beamlab::core::tryParseInteger(cols[static_cast<std::size_t>(col_track)]);
        const auto step_opt  = beamlab::core::tryParseInteger(cols[static_cast<std::size_t>(col_step)]);
        const auto axial_opt = beamlab::core::tryParseFiniteDouble(cols[static_cast<std::size_t>(col_axial)]);
        const auto x_opt     = beamlab::core::tryParseFiniteDouble(cols[static_cast<std::size_t>(col_x)]);
        const auto y_opt     = beamlab::core::tryParseFiniteDouble(cols[static_cast<std::size_t>(col_y)]);
        const auto z_opt     = beamlab::core::tryParseFiniteDouble(cols[static_cast<std::size_t>(col_z)]);
        const auto edep_opt  = beamlab::core::tryParseFiniteDouble(cols[static_cast<std::size_t>(col_edep_MeV)]);
        const auto kine_opt  = beamlab::core::tryParseFiniteDouble(cols[static_cast<std::size_t>(col_kinE)]);

        if (!event_opt || !track_opt || !step_opt || !axial_opt ||
            !x_opt || !y_opt || !z_opt || !edep_opt || !kine_opt) {
            recordDrop("non-finite or non-numeric mandatory field", line_number);
            continue;
        }

        if (*edep_opt < 0.0) { recordDrop("edep_MeV < 0", line_number); continue; }
        if (*kine_opt < 0.0) { recordDrop("kinE_MeV < 0", line_number); continue; }

        BioStep step;
        step.event_id   = static_cast<int>(*event_opt);
        step.track_id   = static_cast<int>(*track_opt);
        step.step_index = static_cast<int>(*step_opt);
        step.axial_m    = *axial_opt;
        step.x_m        = *x_opt;
        step.y_m        = *y_opt;
        step.z_m        = *z_opt;

        step.energy.edep_MeV_original  = *edep_opt;
        step.energy.kinE_MeV_original  = *kine_opt;
        step.energy.edep_MeV_simulated = step.energy.edep_MeV_original;
        step.energy.kinE_MeV_simulated = step.energy.kinE_MeV_original;

        const auto key = std::make_pair(step.event_id, step.track_id);
        auto& bt = track_map[key];  // default-constructs if absent — safe, no dangling
        if (bt.steps.empty()) {
            bt.event_id = step.event_id;
            bt.track_id = step.track_id;
        }
        bt.steps.push_back(std::move(step));
    }

    for (const auto& [reason, stats] : drops) {
        warnings_out.push_back(
            std::string{"BioSim CSV row dropped: "} + reason +
            " (count=" + std::to_string(stats.count) +
            ", first_line=" + std::to_string(stats.first_line) + ")"
        );
    }

    // Move tracks out of the map into the output vector and sort steps.
    tracks.reserve(track_map.size());
    for (auto& [key, bt] : track_map) {
        std::sort(bt.steps.begin(), bt.steps.end(),
            [](const BioStep& a, const BioStep& b){
                return a.step_index < b.step_index;
            });
        tracks.push_back(std::move(bt));
    }

    return !tracks.empty();
}

// ── Process one track ─────────────────────────────────────────────────────────

void BioSimRunner::processTrack(BioTrack& track,
                                 const BioSimConfig& config,
                                 const std::uint64_t master_seed,
                                 std::size_t& non_finite_eloss_counter) const
{
    if (track.steps.empty()) return;

    EnergyStraggling straggling;
    EnergyScaleConverter converter;

    // Build a per-track RNG that depends on (master_seed, event_id, track_id).
    // Using std::seed_seq decorrelates close (event,track) pairs and removes
    // the bug in the legacy seed = event*1000 + track*100 + step_index recipe
    // which collided whenever step_index >= 100 (most tracks).  The state
    // advances naturally between steps — no per-step seed mixing needed.
    std::seed_seq seed_data{
        static_cast<std::uint64_t>(master_seed),
        static_cast<std::uint64_t>(static_cast<std::int64_t>(track.event_id)),
        static_cast<std::uint64_t>(static_cast<std::int64_t>(track.track_id))
    };
    std::mt19937_64 rng(seed_data);

    double current_kinE = track.steps.front().energy.kinE_MeV_original;
    if (!std::isfinite(current_kinE) || current_kinE < 0.0) {
        ++non_finite_eloss_counter;
        current_kinE = 0.0;
    }
    track.entry_kinE_MeV = current_kinE;

    for (std::size_t i = 0; i < track.steps.size(); ++i) {
        auto& step = track.steps[i];

        // Compute step length [cm] from consecutive positions
        double step_len_cm = 0.0;
        if (i + 1 < track.steps.size()) {
            const auto& next = track.steps[i + 1];
            const double dx = next.x_m - step.x_m;
            const double dy = next.y_m - step.y_m;
            const double dz = next.z_m - step.z_m;
            step_len_cm = std::sqrt(dx*dx + dy*dy + dz*dz) * 100.0;
        }

        // Find active slab for this step
        int slab_idx = -1;
        const BioSlab* active_slab = nullptr;
        for (int si = 0; si < static_cast<int>(config.slabs.size()); ++si) {
            const auto& slab = config.slabs[static_cast<std::size_t>(si)];
            if (slab.containsPoint(step.axial_m, step.x_m, step.y_m)) {
                slab_idx = si;
                active_slab = &slab;
                break;
            }
        }

        const BioMaterial* mat_ptr = (active_slab != nullptr) ? &active_slab->material : nullptr;
        double depth_cm = (active_slab != nullptr) ?
            active_slab->depthInSlab_cm(step.axial_m) : 0.0;

        double simulated_edep = step.energy.edep_MeV_original;

        if (active_slab != nullptr && current_kinE > 0.0 && step_len_cm > 0.0) {
            // Recompute energy loss with Bethe-Bloch + straggling
            const auto strag = straggling.sample(
                current_kinE, step_len_cm, active_slab->material,
                config.particle, config.straggling_mode, rng);
            simulated_edep = strag.energy_loss_MeV;
        } else if (active_slab == nullptr) {
            // Outside slabs: preserve Geant4 original value
            simulated_edep = step.energy.edep_MeV_original;
        }

        // Energy-conservation guard: a negative or non-finite eloss is never
        // physical; clamp to 0 and bump the diagnostic counter so downstream
        // (UI, audit log) knows to flag the run.
        if (!std::isfinite(simulated_edep) || simulated_edep < 0.0) {
            simulated_edep = 0.0;
            ++non_finite_eloss_counter;
        }

        const double new_kinE = std::max(0.0, current_kinE - simulated_edep);

        // Populate full EnergyScaleSet
        step.energy = converter.compute(
            step.energy.edep_MeV_original,
            simulated_edep,
            step.energy.kinE_MeV_original,
            new_kinE,
            step_len_cm,
            depth_cm,
            mat_ptr,
            config.particle,
            slab_idx,
            config.beam_radius_cm);

        current_kinE = new_kinE;
    }

    track.exit_kinE_MeV = current_kinE;

    // Track summaries
    for (const auto& step : track.steps) {
        track.total_edep_MeV_original  += step.energy.edep_MeV_original;
        track.total_edep_MeV_simulated += step.energy.edep_MeV_simulated;
        track.total_dose_Gy            += step.energy.dose_Gy;
        track.total_H_mSv              += step.energy.H_mSv;
    }
}

// ── Slab statistics ───────────────────────────────────────────────────────────

std::vector<BioSlabStats> BioSimRunner::buildSlabStats(
    const std::vector<BioTrack>& tracks,
    const BioSimConfig& config,
    std::vector<std::string>& warnings_out) const
{
    std::vector<BioSlabStats> stats;
    stats.resize(config.slabs.size());

    for (std::size_t si = 0; si < config.slabs.size(); ++si) {
        auto& st = stats[si];
        st.slab_id       = config.slabs[si].id;
        st.material_name = config.slabs[si].material.name;
        st.density_g_cm3 = config.slabs[si].material.density_g_cm3;
    }

    for (const auto& track : tracks) {
        // Per-slab: find if this track has any steps in each slab
        std::vector<bool> crossed(config.slabs.size(), false);

        for (const auto& step : track.steps) {
            if (step.energy.slab_index < 0) continue;
            const auto si = static_cast<std::size_t>(step.energy.slab_index);
            if (si >= stats.size()) continue;

            auto& st = stats[si];
            st.total_edep_MeV_original  += step.energy.edep_MeV_original;
            st.total_edep_MeV_simulated += step.energy.edep_MeV_simulated;
            st.total_dose_Gy            += step.energy.dose_Gy;
            st.total_H_mSv              += step.energy.H_mSv;
            st.total_E_mSv              += step.energy.E_mSv;
            st.mean_LET_keV_um          += step.energy.LET_keV_um;
            st.mean_RBE                 += step.energy.RBE;
            st.total_steps_in_slab++;
            crossed[si] = true;
        }

        for (std::size_t si = 0; si < config.slabs.size(); ++si) {
            if (crossed[si]) ++stats[si].tracks_crossing;
        }
    }

    // Finalise means
    for (auto& st : stats) {
        if (st.tracks_crossing > 0) {
            st.mean_edep_per_track_MeV =
                st.total_edep_MeV_simulated / static_cast<double>(st.tracks_crossing);
            st.mean_dose_Gy =
                st.total_dose_Gy / static_cast<double>(st.tracks_crossing);
            st.mean_H_mSv =
                st.total_H_mSv / static_cast<double>(st.tracks_crossing);
        }
        if (st.total_steps_in_slab > 0) {
            st.mean_LET_keV_um /= static_cast<double>(st.total_steps_in_slab);
            st.mean_RBE        /= static_cast<double>(st.total_steps_in_slab);
        }
    }

    // Bragg curves (optional, Bortfeld model — calibrated for protons).
    if (config.compute_bragg_curves && !tracks.empty()) {
        // Bortfeld 1997 was fit to proton depth-dose data in water.  Applying
        // it to muons (or other particles) produces a curve with the right
        // qualitative shape but the wrong peak depth (typical bias ~20-30 %
        // for muons since dE/dx vs depth is much flatter).  Surface a single
        // warning per run instead of silently returning numbers that look
        // authoritative.
        const auto& particle_g4 = config.particle.geant4_name;
        const bool is_muon =
            particle_g4.size() >= 2 &&
            (particle_g4[0] == 'm' || particle_g4[0] == 'M') &&
            (particle_g4[1] == 'u' || particle_g4[1] == 'U');
        if (is_muon) {
            warnings_out.emplace_back(
                "Bortfeld depth-dose curve aplicada a partícula "
                + particle_g4 +
                " — la curva está calibrada para protones; error sistemático "
                "esperado ~20-30 % en la profundidad del Bragg peak."
            );
        }

        BraggPeakCalculator bragg;
        for (std::size_t si = 0; si < config.slabs.size(); ++si) {
            const auto& slab = config.slabs[si];
            if (!slab.enabled) continue;
            // Use mean entry KinE across tracks for the Bortfeld curve
            const double mean_entry_E = tracks.front().entry_kinE_MeV;
            if (mean_entry_E > 0.0) {
                stats[si].bragg_curve = bragg.bortfeldProtonCurve(
                    mean_entry_E, slab.material, config.particle);
                stats[si].bragg_peak_depth_cm =
                    bragg.csdaRange_cm(mean_entry_E, slab.material, config.particle);
            }
        }
    }

    return stats;
}

// ── Global stats ──────────────────────────────────────────────────────────────

void BioSimRunner::computeGlobalStats(BioSimResult& result) const
{
    bool first = true;
    for (const auto& track : result.tracks) {
        result.total_tracks++;
        result.total_steps += track.steps.size();
        for (const auto& step : track.steps) {
            const auto& e = step.energy;
            if (first) {
                result.global_min_kinE_MeV  = e.kinE_MeV_simulated;
                result.global_max_kinE_MeV  = e.kinE_MeV_simulated;
                result.global_min_edep_MeV  = e.edep_MeV_simulated;
                result.global_max_edep_MeV  = e.edep_MeV_simulated;
                result.global_min_LET_keV_um = e.LET_keV_um;
                result.global_max_LET_keV_um = e.LET_keV_um;
                result.global_min_dose_mGy  = e.dose_mGy;
                result.global_max_dose_mGy  = e.dose_mGy;
                first = false;
            } else {
                result.global_min_kinE_MeV  = std::min(result.global_min_kinE_MeV,  e.kinE_MeV_simulated);
                result.global_max_kinE_MeV  = std::max(result.global_max_kinE_MeV,  e.kinE_MeV_simulated);
                result.global_min_edep_MeV  = std::min(result.global_min_edep_MeV,  e.edep_MeV_simulated);
                result.global_max_edep_MeV  = std::max(result.global_max_edep_MeV,  e.edep_MeV_simulated);
                result.global_min_LET_keV_um = std::min(result.global_min_LET_keV_um, e.LET_keV_um);
                result.global_max_LET_keV_um = std::max(result.global_max_LET_keV_um, e.LET_keV_um);
                result.global_min_dose_mGy  = std::min(result.global_min_dose_mGy,  e.dose_mGy);
                result.global_max_dose_mGy  = std::max(result.global_max_dose_mGy,  e.dose_mGy);
            }
        }
    }
}

// ── File hash (Adler-32 as lightweight fingerprint) ───────────────────────────

std::string BioSimRunner::hashFile(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return "unavailable";

    uint32_t a = 1, b = 0;
    constexpr uint32_t MOD_ADLER = 65521;
    char ch{};
    while (f.get(ch)) {
        a = (a + static_cast<unsigned char>(ch)) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }
    const uint32_t checksum = (b << 16) | a;

    // Format as 8-char hex
    char buf[20];
    std::snprintf(buf, sizeof(buf), "adler32:%08X", checksum);
    return std::string(buf);
}

// ── Main run ──────────────────────────────────────────────────────────────────

BioSimResult BioSimRunner::run(const BioSimConfig& config,
                                ProgressCallback progress_cb) const
{
    BioSimResult result;

    // 1. Load CSV
    std::string err;
    if (!loadCsv(config.energy_step_csv_path, result.tracks, err, result.warnings)) {
        result.message = "CSV load failed: " + err;
        return result;
    }

    result.source_csv_sha256 = hashFile(config.energy_step_csv_path);

    // Resolve the master seed.  rng_seed=0 means "auto" → mint a fresh one
    // from std::random_device and report it in result.effective_seed so the
    // run can be reproduced exactly later.
    if (config.rng_seed != 0) {
        result.effective_seed = config.rng_seed;
    } else {
        std::random_device rd;
        const std::uint64_t hi = static_cast<std::uint64_t>(rd()) << 32;
        const std::uint64_t lo = static_cast<std::uint64_t>(rd());
        std::uint64_t seed = hi | lo;
        if (seed == 0) seed = 0x9E3779B97F4A7C15ULL; // golden-ratio fallback
        result.effective_seed = seed;
    }

    // 2. Process each track
    const std::size_t N = result.tracks.size();
    for (std::size_t i = 0; i < N; ++i) {
        if (progress_cb) {
            const int pct = static_cast<int>((i * 100) / N);
            if (!progress_cb(pct)) {
                result.message = "Simulation aborted by user.";
                return result;
            }
        }
        processTrack(result.tracks[i], config,
                     result.effective_seed,
                     result.non_finite_eloss_steps);
    }

    if (result.non_finite_eloss_steps > 0) {
        result.warnings.emplace_back(
            "Energy-loss guard se activó en " +
            std::to_string(result.non_finite_eloss_steps) +
            " paso(s) (eloss negativo o no finito clampeado a 0)."
        );
    }

    // 3. Build slab statistics
    result.slab_stats = buildSlabStats(result.tracks, config, result.warnings);

    // 4. Global stats for colormap scaling
    computeGlobalStats(result);

    // 5. Timestamp
    const auto now = std::chrono::system_clock::now();
    const auto t   = std::chrono::system_clock::to_time_t(now);
    char tbuf[32];
    std::strftime(tbuf, sizeof(tbuf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    result.timestamp = tbuf;

    if (progress_cb) progress_cb(100);

    result.valid   = true;
    result.message = "Simulation complete: " + std::to_string(result.total_tracks) +
                     " tracks, " + std::to_string(config.slabs.size()) + " slab(s).";
    return result;
}

} // namespace beamlab::biosim
