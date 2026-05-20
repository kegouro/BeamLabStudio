#!/usr/bin/env python3
"""Generate synthetic Geant4 stepping CSV (tab-separated, 15 columns)."""
import argparse, math, os, sys, time

HEADER = "x_cm\ty_cm\tz_cm\tedep_MeV\tkinE_MeV\tmomx_MeV\tmomy_MeV\tmomz_MeV\ttime_ns\ttrackID\tparentID\teventID\tpdg\tparticleName\tsource_file"

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--rows", type=int, default=5_000_000, help="number of data rows")
    p.add_argument("--output", default="/tmp/test_1gb.csv", help="output CSV path")
    p.add_argument("--tracks", type=int, default=50, help="unique tracks per event")
    p.add_argument("--seed", type=int, default=42)
    args = p.parse_args()

    # Each row ~200 bytes → 5M rows ≈ 1 GB
    total_bytes_est = args.rows * 200
    print(f"Writing {args.rows:,} rows → ~{total_bytes_est/1e9:.1f} GB to {args.output}")

    import random
    random.seed(args.seed)
    t0 = time.time()

    with open(args.output, "w") as f:
        f.write(HEADER + "\n")
        for i in range(args.rows):
            ev = i // args.tracks  # eventID
            tk = i % args.tracks   # trackID
            z = 1000.0 + (i % 50000) * 0.02  # z grows slowly
            kine = max(0.1, 2000.0 - (i % 50000) * 0.04)  # kinE decreases
            edep = 0.01 + random.random() * 0.03
            mom_mag = kine * 1.05
            momz = mom_mag * 0.95
            momx = (random.random() - 0.5) * 50
            momy = (random.random() - 0.5) * 50
            pdg = -13  # mu+
            time_ns = i * 0.033  # ~30ps per step

            f.write(f"{0:.6f}\t{0:.6f}\t{z:.6f}\t{edep:.6f}\t{kine:.6f}\t"
                    f"{momx:.6f}\t{momy:.6f}\t{momz:.6f}\t{time_ns:.6f}\t"
                    f"{tk}\t0\t{ev}\t{pdg}\tmu+\tstepping_data_t0.root\n")
            if (i + 1) % 1_000_000 == 0:
                pct = (i + 1) / args.rows * 100
                elapsed = time.time() - t0
                rate = (i + 1) / elapsed
                eta = (args.rows - i - 1) / rate
                print(f"  {i+1:,} rows ({pct:.0f}%) — {rate:.0f} rows/s — ETA {eta:.0f}s")

    elapsed = time.time() - t0
    size_mb = os.path.getsize(args.output) / 1e6
    print(f"\nDone: {args.rows:,} rows in {elapsed:.1f}s ({size_mb:.0f} MB)")
    print(f"Output: {args.output}")

if __name__ == "__main__":
    main()
