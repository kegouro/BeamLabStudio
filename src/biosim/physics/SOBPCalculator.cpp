// SOBPCalculator — Spread-Out Bragg Peak with analytic weight computation.
//
// Weight algorithm — back-substitution (distal → proximal):
//   Each component k is a Bragg curve whose range equals its sample depth z_k,
//   so D_k peaks at z_k and is ≈ 0 beyond it.  The dose matrix D_j(z_k) is
//   therefore effectively lower triangular, and the constraint SOBP(z_k) = 1
//   can be solved without any linear solver by sweeping from the deepest peak
//   inward:
//       w_k = max(0, (1 − Σ_{j>k} w_j·D_j(z_k)) / D_k(z_k)),  k = N−1 … 0.
//   This yields non-negative weights by construction (no clamp of a solver's
//   negative output) and a flat plateau; flatness improves with more peaks.
//
//   Reference:
//     T. Bortfeld, W. Schlegel, "An analytical approximation of depth-dose
//     distributions for therapeutic proton beams",
//     Phys. Med. Biol. 41 (1996) 1331–1339.
//
// Energy-range inversion uses bisection on csdaRange_cm.

#include "biosim/physics/SOBPCalculator.h"

#include <algorithm>
#include <cmath>

namespace beamlab::biosim {

// ── Constructor ───────────────────────────────────────────────────────────────

SOBPCalculator::SOBPCalculator()
    : water_(BioMaterialLibrary::water())
    , proton_(ParticleLibrary::proton())
{}

// ── energyForRange_cm — bisection inversion of csdaRange_cm ──────────────────

double SOBPCalculator::energyForRange_cm(
    const double target_cm,
    const double E_lo_MeV,
    const double E_hi_MeV,
    const double tol_cm) const
{
    if (target_cm <= 0.0) return 0.0;

    // Sanity-check that the range brackets the target.
    const double r_lo = calc_.csdaRange_cm(E_lo_MeV, water_, proton_);
    const double r_hi = calc_.csdaRange_cm(E_hi_MeV, water_, proton_);

    if (target_cm < r_lo || target_cm > r_hi) return 0.0;

    // Bisection: find E such that csdaRange_cm(E) = target_cm.
    double lo = E_lo_MeV;
    double hi = E_hi_MeV;
    constexpr int k_max_iter = 60;

    for (int i = 0; i < k_max_iter; ++i) {
        const double mid = 0.5 * (lo + hi);
        const double r   = calc_.csdaRange_cm(mid, water_, proton_);
        if (std::abs(r - target_cm) < tol_cm) return mid;
        if (r < target_cm) lo = mid;
        else               hi = mid;
    }
    return 0.5 * (lo + hi);
}

// ── resampleCurve — linear interpolation onto uniform grid ───────────────────

std::vector<double> SOBPCalculator::resampleCurve(
    const std::vector<BraggPeakCalculator::BraggCurvePoint>& curve,
    const std::vector<double>& grid)
{
    std::vector<double> out(grid.size(), 0.0);
    if (curve.empty()) return out;

    const std::size_t nc = curve.size();
    std::size_t j = 0; // cursor into curve (monotone scan)

    for (std::size_t i = 0; i < grid.size(); ++i) {
        const double z = grid[i];

        // Before or beyond the curve: zero.
        if (z < curve.front().depth_cm || z > curve.back().depth_cm) {
            out[i] = 0.0;
            continue;
        }

        // Advance j so that curve[j].depth_cm <= z <= curve[j+1].depth_cm.
        while (j + 1 < nc && curve[j + 1].depth_cm <= z) ++j;
        if (j + 1 >= nc) {
            out[i] = curve.back().dose_rel;
            continue;
        }

        const double z0 = curve[j].depth_cm;
        const double z1 = curve[j + 1].depth_cm;
        const double dz = z1 - z0;
        if (dz <= 0.0) {
            out[i] = curve[j].dose_rel;
            continue;
        }
        const double t = (z - z0) / dz;
        out[i] = (1.0 - t) * curve[j].dose_rel + t * curve[j + 1].dose_rel;
    }
    return out;
}

// ── compute ───────────────────────────────────────────────────────────────────

SOBPResult SOBPCalculator::compute(const SOBPParams& p) const
{
    SOBPResult result;
    result.proximal_cm = p.proximal_cm;
    result.distal_cm   = p.distal_cm;

    // --- Input validation ---------------------------------------------------
    if (p.n_peaks < 1) {
        result.error = "n_peaks must be >= 1";
        return result;
    }
    if (p.proximal_cm <= 0.0 || p.distal_cm <= p.proximal_cm) {
        result.error = "Need 0 < proximal_cm < distal_cm";
        return result;
    }
    if (p.energy_max_MeV <= 0.0) {
        result.error = "energy_max_MeV must be > 0";
        return result;
    }
    if (p.depth_step_cm <= 0.0) {
        result.error = "depth_step_cm must be > 0";
        return result;
    }

    // --- Distal range from max energy --------------------------------------
    const double range_distal = calc_.csdaRange_cm(p.energy_max_MeV, water_, proton_);
    if (range_distal <= 0.0) {
        result.error = "csdaRange_cm returned 0 for given energy";
        return result;
    }

    // If the user's distal_cm exceeds the CSDA range of max energy, cap it.
    const double distal_cm = std::min(p.distal_cm, range_distal);
    const double proximal_cm = p.proximal_cm;

    if (distal_cm <= proximal_cm) {
        result.error = "Requested distal depth exceeds proton range for given "
                       "energy_max_MeV; increase energy_max_MeV or reduce distal_cm";
        return result;
    }

    // --- Place N Bragg-peak maxima across the plateau ----------------------
    // The k-th component is a Bragg curve whose *maximum* must land at the
    // sample depth peak_pos[k].  The deepest maxima are pushed one half-spacing
    // beyond distal_cm so that the deepest peak's distal fall-off lies *outside*
    // [proximal, distal]; otherwise the plateau would slope down near its distal
    // edge.  (Standard SOBP design: the distal 100 % point sits a little past
    // the prescribed distal depth.)
    const int N = p.n_peaks;

    // A pristine Bortfeld curve peaks at f·R0 with f slightly below 1 (range
    // straggling shifts the maximum proximally).  Measure f once at the deepest
    // energy so component ranges can be set as range = peak_pos / f.
    double peak_frac = 0.99; // sensible default; refined below
    {
        const auto probe = calc_.bortfeldProtonCurve(
            p.energy_max_MeV, water_, proton_, 2000, p.sigma_E_rel);
        std::size_t pmax_i = 0;
        double      pmax_v = 0.0;
        for (std::size_t i = 0; i < probe.size(); ++i) {
            if (probe[i].dose_rel > pmax_v) { pmax_v = probe[i].dose_rel; pmax_i = i; }
        }
        if (!probe.empty() && range_distal > 0.0) {
            peak_frac = std::clamp(probe[pmax_i].depth_cm / range_distal, 0.80, 1.0);
        }
    }

    const double spacing = (N > 1)
        ? (distal_cm - proximal_cm) / static_cast<double>(N - 1)
        : (distal_cm - proximal_cm);

    std::vector<double> peak_pos(static_cast<std::size_t>(N));
    if (N == 1) {
        peak_pos[0] = distal_cm;
    } else {
        // ponytail: half-spacing distal margin keeps the fall-off off-plateau.
        //   techo: uniform spacing | upgrade: adaptive (Jette-Chen) spacing.
        constexpr double k_distal_margin = 0.5; // in units of peak spacing
        const double hi = distal_cm + k_distal_margin * spacing;
        for (int k = 0; k < N; ++k) {
            peak_pos[static_cast<std::size_t>(k)] =
                proximal_cm + static_cast<double>(k)
                * (hi - proximal_cm) / static_cast<double>(N - 1);
        }
    }

    // --- Find energies so each component's range = peak_pos / peak_frac ----
    std::vector<double> energies(static_cast<std::size_t>(N));
    std::vector<double> ranges(static_cast<std::size_t>(N));
    for (int k = 0; k < N; ++k) {
        const std::size_t uk = static_cast<std::size_t>(k);
        ranges[uk] = peak_pos[uk] / peak_frac;
        const double E = energyForRange_cm(ranges[uk]);
        if (E <= 0.0) {
            result.error = "energyForRange_cm failed for depth "
                           + std::to_string(peak_pos[uk]) + " cm";
            return result;
        }
        energies[uk] = E;
    }

    // --- Build shared depth grid -------------------------------------------
    // Extend past the deepest component's range so its tail is visible.
    const double range_deepest = *std::max_element(ranges.begin(), ranges.end());
    const double z_max  = std::max(range_distal, range_deepest) * 1.12;
    const int n_grid = static_cast<int>(std::ceil(z_max / p.depth_step_cm)) + 1;
    std::vector<double> depth_grid(static_cast<std::size_t>(n_grid));
    for (int i = 0; i < n_grid; ++i) {
        depth_grid[static_cast<std::size_t>(i)] =
            static_cast<double>(i) * p.depth_step_cm;
    }

    // --- Per-component energy spread matched to the peak spacing -----------
    // Sharp pristine peaks (FWHM ≪ spacing) cannot tile a wide plateau flatly,
    // so widen each curve to σ_target ≈ 0.45·spacing.  The Bohr straggling term
    // (0.012·√R0, fixed physics) is already present; the remaining width is
    // supplied through the energy-spread σ_E:
    //   σ_target² = (0.012·√R0)² + (σ_E_rel·R0)²  ⇒ solve for σ_E_rel.
    // ponytail: 0.45 chosen empirically (plateau ripple < 5 %).
    //   techo: σ_target ∝ spacing | upgrade: per-energy emittance model.
    constexpr double k_width_frac = 0.45;
    const double sigma_target_cm = std::max(k_width_frac * spacing, 1e-3);

    // --- Compute and resample each (widened) Bortfeld curve ----------------
    std::vector<std::vector<double>> curves(static_cast<std::size_t>(N));
    for (int k = 0; k < N; ++k) {
        const std::size_t uk = static_cast<std::size_t>(k);
        const double R0       = ranges[uk];
        const double straggle = 0.012 * std::sqrt(R0);
        const double extra    = sigma_target_cm * sigma_target_cm
                                - straggle * straggle;
        const double sigma_E_rel = (extra > 0.0 && R0 > 0.0)
            ? std::sqrt(extra) / R0
            : p.sigma_E_rel;
        const auto raw = calc_.bortfeldProtonCurve(
            energies[uk], water_, proton_, p.curve_n_points, sigma_E_rel);
        curves[uk] = resampleCurve(raw, depth_grid);
    }

    // --- Initial weights by back-substitution (distal → proximal) ---------
    //
    // We want SOBP(z_k) = Σ_j w_j·D_j(z_k) = 1 at each peak depth z_k.
    // Component j peaks at z_j and is ≈ 0 for z > z_j, so the deepest peak
    // (k = N−1) dominates at z_{N-1}, the next at z_{N-2}, and so on: the dose
    // matrix is effectively lower triangular and the constraint can be solved
    // by back-substitution from the distal peak inward — no linear solver, and
    // the weights are non-negative by construction:
    //
    //   w_k = max(0, (1 − Σ_{j>k} w_j·D_j(z_k)) / D_k(z_k)),  k = N−1 … 0.
    //
    // This already gives a flat plateau at the peak depths; the least-squares
    // sweep below removes the residual inter-peak ripple.
    //
    // Reference: T. Bortfeld, W. Schlegel, "An analytical approximation of
    //   depth-dose distributions for therapeutic proton beams",
    //   Phys. Med. Biol. 41 (1996) 1331-1339.
    auto depthToIdx = [&](double z_cm) -> std::size_t {
        if (p.depth_step_cm <= 0.0) return 0;
        const auto idx = static_cast<std::size_t>(std::round(z_cm / p.depth_step_cm));
        return std::min(idx, static_cast<std::size_t>(n_grid - 1));
    };

    const auto un = static_cast<std::size_t>(N);

    // Cache the peak-depth grid indices (each component's own maximum location).
    std::vector<std::size_t> idx_of(un);
    for (int k = 0; k < N; ++k) {
        idx_of[static_cast<std::size_t>(k)] =
            depthToIdx(peak_pos[static_cast<std::size_t>(k)]);
    }

    std::vector<double> weights(un, 0.0);
    for (int k = N - 1; k >= 0; --k) {
        const std::size_t uk    = static_cast<std::size_t>(k);
        const std::size_t idx_k = idx_of[uk];
        double residual = 1.0;
        for (int j = k + 1; j < N; ++j) {
            const std::size_t uj = static_cast<std::size_t>(j);
            residual -= weights[uj] * curves[uj][idx_k];
        }
        const double Dk = curves[uk][idx_k];
        weights[uk] = (Dk > 0.0) ? std::max(0.0, residual / Dk) : 0.0;
    }
    if (N == 1) weights[0] = 1.0; // single peak carries the full SOBP

    // --- Refine weights: non-negative least-squares over the plateau -------
    // Back-substitution forces unity only at the N peak depths; between them a
    // few-percent ripple remains.  Polish with a projected Gauss-Seidel sweep
    // that minimises Σ_{z∈plateau} (SOBP(z) − 1)² subject to w_k ≥ 0.  This is
    // the discrete analogue of the Bortfeld-Schlegel deconvolution weighting,
    // keeps weights non-negative by construction (no clamp of garbage), and
    // needs no general linear solver.  It converges in a few dozen sweeps for
    // N ≤ 30.  ponytail: coordinate descent | techo: N ≤ 30 | upgrade: Lawson-Hanson NNLS.
    if (N > 1) {
        // Plateau grid index range [i_lo, i_hi].
        std::size_t i_lo = 0;
        std::size_t i_hi = static_cast<std::size_t>(n_grid) - 1;
        while (i_lo < i_hi && depth_grid[i_lo]     < proximal_cm) ++i_lo;
        while (i_hi > i_lo && depth_grid[i_hi]     > distal_cm)   --i_hi;

        constexpr int k_max_sweeps = 500;
        constexpr double k_tol = 1e-9;
        for (int sweep = 0; sweep < k_max_sweeps; ++sweep) {
            double max_change = 0.0;
            for (int k = 0; k < N; ++k) {
                const std::size_t uk = static_cast<std::size_t>(k);
                // Solve dE/dw_k = 0 for w_k holding the others fixed:
                //   w_k = Σ_i D_k(z_i)·(1 − Σ_{j≠k} w_j·D_j(z_i)) / Σ_i D_k(z_i)².
                double num = 0.0;
                double den = 0.0;
                for (std::size_t i = i_lo; i <= i_hi; ++i) {
                    const double a = curves[uk][i];
                    if (a == 0.0) continue;
                    double rest = 0.0;
                    for (int j = 0; j < N; ++j) {
                        if (j == k) continue;
                        rest += weights[static_cast<std::size_t>(j)] * curves[static_cast<std::size_t>(j)][i];
                    }
                    num += a * (1.0 - rest);
                    den += a * a;
                }
                const double w_new = (den > 0.0) ? std::max(0.0, num / den) : 0.0;
                max_change = std::max(max_change, std::abs(w_new - weights[uk]));
                weights[uk] = w_new;
            }
            if (max_change < k_tol) break;
        }
    }

    // --- Assemble SOBP(z) --------------------------------------------------
    std::vector<double> sobp(static_cast<std::size_t>(n_grid), 0.0);
    for (int k = 0; k < N; ++k) {
        const std::size_t uk = static_cast<std::size_t>(k);
        for (int i = 0; i < n_grid; ++i) {
            const std::size_t ui = static_cast<std::size_t>(i);
            sobp[ui] += weights[uk] * curves[uk][ui];
        }
    }

    // Normalise SOBP to global max = 1 (header contract / single-peak test).
    // The same factor scales the individual components so that, after
    // normalisation, Σ_k weighted_dose[k] == sobp at every depth.
    const double sobp_max   = *std::max_element(sobp.begin(), sobp.end());
    const double sobp_scale = (sobp_max > 0.0) ? 1.0 / sobp_max : 1.0;
    for (auto& v : sobp) v *= sobp_scale;

    // --- Plateau flatness ---------------------------------------------------
    // With the SOBP normalised to max = 1, flatness is the largest departure
    // from unity over the prescribed plateau:
    //   flatness = max|SOBP(z) − 1|  for z ∈ [proximal, distal].
    // ≤ 0.10 means the plateau is flat within ±10 % of the global maximum.
    {
        double max_dev = 0.0;
        for (int i = 0; i < n_grid; ++i) {
            const std::size_t ui = static_cast<std::size_t>(i);
            const double z = depth_grid[ui];
            if (z >= proximal_cm && z <= distal_cm) {
                max_dev = std::max(max_dev, std::abs(sobp[ui] - 1.0));
            }
        }
        result.plateau_max_deviation = max_dev;
    }

    // --- Populate components -----------------------------------------------
    result.components.resize(static_cast<std::size_t>(N));
    for (int k = 0; k < N; ++k) {
        const std::size_t uk = static_cast<std::size_t>(k);
        auto& comp        = result.components[uk];
        comp.energy_MeV   = energies[uk];
        comp.weight       = weights[uk];
        comp.range_cm     = ranges[uk]; // CSDA range of this component [cm]
        comp.weighted_dose.resize(static_cast<std::size_t>(n_grid));
        for (int i = 0; i < n_grid; ++i) {
            const std::size_t ui = static_cast<std::size_t>(i);
            comp.weighted_dose[ui] = weights[uk] * curves[uk][ui] * sobp_scale;
        }
    }

    result.depth_grid_cm = std::move(depth_grid);
    result.sobp_dose     = std::move(sobp);
    result.valid         = true;
    return result;
}

} // namespace beamlab::biosim
