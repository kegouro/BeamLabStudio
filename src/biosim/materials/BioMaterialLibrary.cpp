#include "biosim/materials/BioMaterialLibrary.h"

#include <algorithm>
#include <cctype>

namespace beamlab::biosim {

namespace {

std::string toLower(std::string s)
{
    for (auto& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

} // namespace

// ── Biological tissues (ICRU-44) ─────────────────────────────────────────────

BioMaterial BioMaterialLibrary::water()
{
    BioMaterial m;
    m.id = "water_icru";
    m.name = "Water (ICRU-44)";
    m.symbol = "H₂O";
    m.category = "biological";
    m.reference = "ICRU Report 44 (1989), Table A.1";
    m.density_g_cm3 = 1.000;
    m.Z_eff = 7.22;
    m.A_eff = 13.00;  // gives Z/A = 0.5554 matching Σ(w_i·Z_i/A_i) for H₂O
    m.I_eV = 75.0;
    m.I_eV_uncertainty = 3.0;
    m.sternheimer_a = 0.09116;
    m.sternheimer_k = 3.4773;
    m.sternheimer_x0 = 0.2400;
    m.sternheimer_x1 = 2.8004;
    m.sternheimer_C_delta = -3.5017;
    m.sternheimer_delta0 = 0.0;
    m.has_sternheimer = true;
    m.radiation_length_cm = 36.08;
    m.nuclear_int_length_cm = 83.30;
    m.moliere_radius_cm = 9.00;
    m.WR = 1.0;
    m.WT = 0.0;
    m.alpha_beta_ratio = 10.0;
    m.state = 1; // liquid
    m.melting_point_K = 273.15;
    m.boiling_point_K = 373.15;
    m.refraction_index = 1.333;
    m.density_uncertainty_rel = 0.001;
    m.composition = {{"H", 1, 0.111898}, {"O", 8, 0.888102}};
    return m;
}

BioMaterial BioMaterialLibrary::muscleSkeletalICRU()
{
    BioMaterial m;
    m.id = "muscle_skeletal";
    m.name = "Skeletal Muscle (ICRU-44)";
    m.symbol = "Muscle";
    m.category = "biological";
    m.reference = "ICRU Report 44 (1989), Table A.1";
    m.density_g_cm3 = 1.050;
    m.Z_eff = 7.64;
    m.A_eff = 13.19;
    m.I_eV = 74.7;
    m.I_eV_uncertainty = 4.0;
    m.sternheimer_a = 0.08507;
    m.sternheimer_k = 3.5162;
    m.sternheimer_x0 = 0.2000;
    m.sternheimer_x1 = 2.8000;
    m.sternheimer_C_delta = -3.5330;
    m.sternheimer_delta0 = 0.0;
    m.has_sternheimer = true;
    m.radiation_length_cm = 35.57;
    m.nuclear_int_length_cm = 81.50;
    m.moliere_radius_cm = 9.20;
    m.WR = 1.0;
    m.WT = 0.12;
    m.alpha_beta_ratio = 10.0;
    m.organ_name = "muscle";
    m.state = 1;
    m.composition = {
        {"H",  1,  0.100637},
        {"C",  6,  0.107830},
        {"N",  7,  0.027680},
        {"O",  8,  0.754773},
        {"Na", 11, 0.000750},
        {"Mg", 12, 0.000190},
        {"P",  15, 0.001800},
        {"S",  16, 0.002410},
        {"Cl", 17, 0.000790},
        {"K",  19, 0.003020},
        {"Ca", 20, 0.000030},
        {"Zn", 30, 0.000050},
        {"Fe", 26, 0.000040}
    };
    return m;
}

BioMaterial BioMaterialLibrary::boneCorticalICRU()
{
    BioMaterial m;
    m.id = "bone_cortical";
    m.name = "Cortical Bone (ICRU-44)";
    m.symbol = "Bone";
    m.category = "biological";
    m.reference = "ICRU Report 44 (1989), Table A.1";
    m.density_g_cm3 = 1.920;
    m.Z_eff = 13.80;
    m.A_eff = 22.39;
    m.I_eV = 106.4;
    m.I_eV_uncertainty = 5.0;
    m.sternheimer_a = 0.09627;
    m.sternheimer_k = 3.4176;
    m.sternheimer_x0 = 0.1000;
    m.sternheimer_x1 = 2.0000;
    m.sternheimer_C_delta = -3.7738;
    m.sternheimer_delta0 = 0.0;
    m.has_sternheimer = true;
    m.radiation_length_cm = 17.59;
    m.nuclear_int_length_cm = 65.10;
    m.moliere_radius_cm = 5.90;
    m.WR = 1.0;
    m.WT = 0.01;
    m.alpha_beta_ratio = 3.0;
    m.organ_name = "bone surface";
    m.state = 0;
    m.composition = {
        {"H",  1,  0.034000},
        {"C",  6,  0.155000},
        {"N",  7,  0.042000},
        {"O",  8,  0.435000},
        {"Mg", 12, 0.002200},
        {"P",  15, 0.103000},
        {"S",  16, 0.003000},
        {"Ca", 20, 0.225000},
        {"Zn", 30, 0.000100}
    };
    return m;
}

BioMaterial BioMaterialLibrary::boneSpongy()
{
    BioMaterial m;
    m.id = "bone_spongy";
    m.name = "Spongy Bone (ICRU-44)";
    m.symbol = "Spongy";
    m.category = "biological";
    m.reference = "ICRU Report 44 (1989)";
    m.density_g_cm3 = 1.180;
    m.Z_eff = 10.22;
    m.A_eff = 17.10;
    m.I_eV = 89.9;
    m.has_sternheimer = false;
    m.radiation_length_cm = 22.80;
    m.nuclear_int_length_cm = 71.30;
    m.WR = 1.0;
    m.WT = 0.12; // red bone marrow
    m.alpha_beta_ratio = 10.0;
    m.organ_name = "red bone marrow";
    m.state = 0;
    m.composition = {
        {"H",  1,  0.063984},
        {"C",  6,  0.261692},
        {"N",  7,  0.039072},
        {"O",  8,  0.436476},
        {"Mg", 12, 0.001100},
        {"P",  15, 0.061000},
        {"S",  16, 0.001800},
        {"Ca", 20, 0.134000}
    };
    return m;
}

BioMaterial BioMaterialLibrary::adiposeTissue()
{
    BioMaterial m;
    m.id = "adipose_tissue";
    m.name = "Adipose Tissue (ICRU-44)";
    m.symbol = "Fat";
    m.category = "biological";
    m.reference = "ICRU Report 44 (1989), Table A.1";
    m.density_g_cm3 = 0.950;
    m.Z_eff = 6.46;
    m.A_eff = 11.00;
    m.I_eV = 63.2;
    m.I_eV_uncertainty = 3.0;
    m.sternheimer_a = 0.09838;
    m.sternheimer_k = 3.4279;
    m.sternheimer_x0 = 0.1827;
    m.sternheimer_x1 = 2.6710;
    m.sternheimer_C_delta = -3.5019;
    m.sternheimer_delta0 = 0.0;
    m.has_sternheimer = true;
    m.radiation_length_cm = 38.85;
    m.nuclear_int_length_cm = 83.00;
    m.WR = 1.0;
    m.WT = 0.05;
    m.alpha_beta_ratio = 10.0;
    m.organ_name = "adipose";
    m.state = 1;
    m.composition = {
        {"H",  1,  0.119477},
        {"C",  6,  0.637240},
        {"N",  7,  0.007970},
        {"O",  8,  0.232333},
        {"Na", 11, 0.000500},
        {"S",  16, 0.000730},
        {"Cl", 17, 0.001190},
        {"K",  19, 0.000320},
        {"Ca", 20, 0.000240}
    };
    return m;
}

BioMaterial BioMaterialLibrary::brainGrey()
{
    BioMaterial m;
    m.id = "brain_grey";
    m.name = "Brain — Grey Matter (ICRU-44)";
    m.symbol = "BrainGM";
    m.category = "biological";
    m.reference = "ICRU Report 44 (1989), Table A.1";
    m.density_g_cm3 = 1.045;
    m.Z_eff = 7.57;
    m.A_eff = 13.01;
    m.I_eV = 73.9;
    m.I_eV_uncertainty = 4.0;
    m.sternheimer_a = 0.08609;
    m.sternheimer_k = 3.5108;
    m.sternheimer_x0 = 0.2200;
    m.sternheimer_x1 = 2.8000;
    m.sternheimer_C_delta = -3.5272;
    m.sternheimer_delta0 = 0.0;
    m.has_sternheimer = true;
    m.radiation_length_cm = 36.01;
    m.nuclear_int_length_cm = 81.80;
    m.WR = 1.0;
    m.WT = 0.01;
    m.alpha_beta_ratio = 2.0; // CNS: low α/β, late-responding
    m.organ_name = "brain";
    m.state = 1;
    m.composition = {
        {"H",  1,  0.106800},
        {"C",  6,  0.095200},
        {"N",  7,  0.015300},
        {"O",  8,  0.761200},
        {"Na", 11, 0.001840},
        {"P",  15, 0.003540},
        {"S",  16, 0.001830},
        {"Cl", 17, 0.002360},
        {"K",  19, 0.003100},
        {"Ca", 20, 0.000090},
        {"Fe", 26, 0.000050},
        {"Zn", 30, 0.000130}
    };
    return m;
}

BioMaterial BioMaterialLibrary::brainWhite()
{
    BioMaterial m;
    m.id = "brain_white";
    m.name = "Brain — White Matter (ICRU-44)";
    m.symbol = "BrainWM";
    m.category = "biological";
    m.reference = "ICRU Report 44 (1989), Table A.1";
    m.density_g_cm3 = 1.041;
    m.Z_eff = 7.52;
    m.A_eff = 12.89;
    m.I_eV = 73.9;
    m.has_sternheimer = false;
    m.radiation_length_cm = 36.10;
    m.WR = 1.0;
    m.WT = 0.01;
    m.alpha_beta_ratio = 2.0;
    m.organ_name = "brain";
    m.state = 1;
    m.composition = {
        {"H",  1,  0.106800},
        {"C",  6,  0.143200},
        {"N",  7,  0.015600},
        {"O",  8,  0.712700},
        {"Na", 11, 0.001580},
        {"Mg", 12, 0.000130},
        {"P",  15, 0.003540},
        {"S",  16, 0.001640},
        {"Cl", 17, 0.001760},
        {"K",  19, 0.002640},
        {"Ca", 20, 0.000090},
        {"Fe", 26, 0.000050},
        {"Zn", 30, 0.000130}
    };
    return m;
}

BioMaterial BioMaterialLibrary::lungInflated()
{
    BioMaterial m;
    m.id = "lung_inflated";
    m.name = "Lung Tissue — Inflated (ICRU-44)";
    m.symbol = "Lung";
    m.category = "biological";
    m.reference = "ICRU Report 44 (1989), Table A.1";
    m.density_g_cm3 = 0.260;
    m.Z_eff = 7.64;
    m.A_eff = 13.19;
    m.I_eV = 74.9;
    m.I_eV_uncertainty = 4.0;
    m.sternheimer_a = 0.08507;
    m.sternheimer_k = 3.5162;
    m.sternheimer_x0 = 0.2000;
    m.sternheimer_x1 = 2.8000;
    m.sternheimer_C_delta = -3.5330;
    m.sternheimer_delta0 = 0.0;
    m.has_sternheimer = true;
    m.radiation_length_cm = 138.3; // ρ-scaled from muscle
    m.nuclear_int_length_cm = 313.5;
    m.WR = 1.0;
    m.WT = 0.12;
    m.alpha_beta_ratio = 10.0;
    m.organ_name = "lung";
    m.state = 1;
    m.composition = {
        {"H",  1,  0.101278},
        {"C",  6,  0.102310},
        {"N",  7,  0.028650},
        {"O",  8,  0.757072},
        {"Na", 11, 0.001840},
        {"P",  15, 0.000800},
        {"S",  16, 0.002250},
        {"Cl", 17, 0.002660},
        {"K",  19, 0.001940},
        {"Ca", 20, 0.000090},
        {"Fe", 26, 0.000370},
        {"Zn", 30, 0.000010}
    };
    return m;
}

BioMaterial BioMaterialLibrary::lungDeflated()
{
    BioMaterial m = lungInflated();
    m.id = "lung_deflated";
    m.name = "Lung Tissue — Deflated (ICRU-44)";
    m.density_g_cm3 = 1.050;
    m.radiation_length_cm = 35.57;
    m.nuclear_int_length_cm = 81.50;
    return m;
}

BioMaterial BioMaterialLibrary::blood()
{
    BioMaterial m;
    m.id = "blood_whole";
    m.name = "Blood — Whole Blood (ICRU-44)";
    m.symbol = "Blood";
    m.category = "biological";
    m.reference = "ICRU Report 44 (1989), Table A.1";
    m.density_g_cm3 = 1.060;
    m.Z_eff = 7.63;
    m.A_eff = 13.17;
    m.I_eV = 75.2;
    m.has_sternheimer = false;
    m.radiation_length_cm = 35.60;
    m.WR = 1.0;
    m.WT = 0.0;
    m.state = 1;
    m.composition = {
        {"H",  1,  0.101866},
        {"C",  6,  0.100020},
        {"N",  7,  0.029640},
        {"O",  8,  0.759400},
        {"Na", 11, 0.001850},
        {"Mg", 12, 0.000040},
        {"Si", 14, 0.000030},
        {"P",  15, 0.000350},
        {"S",  16, 0.001850},
        {"Cl", 17, 0.002780},
        {"K",  19, 0.001630},
        {"Ca", 20, 0.000060},
        {"Fe", 26, 0.000460},
        {"Zn", 30, 0.000010}
    };
    return m;
}

BioMaterial BioMaterialLibrary::skin()
{
    BioMaterial m;
    m.id = "skin";
    m.name = "Skin (ICRU-44)";
    m.symbol = "Skin";
    m.category = "biological";
    m.reference = "ICRU Report 44 (1989), Table A.1";
    m.density_g_cm3 = 1.090;
    m.Z_eff = 7.48;
    m.A_eff = 12.78;
    m.I_eV = 72.7;
    m.has_sternheimer = false;
    m.radiation_length_cm = 36.30;
    m.WR = 1.0;
    m.WT = 0.01;
    m.organ_name = "skin";
    m.state = 0;
    m.composition = {
        {"H",  1,  0.100588},
        {"C",  6,  0.228250},
        {"N",  7,  0.046420},
        {"O",  8,  0.619002},
        {"Na", 11, 0.000070},
        {"Mg", 12, 0.000060},
        {"P",  15, 0.000330},
        {"S",  16, 0.001590},
        {"Cl", 17, 0.002670},
        {"K",  19, 0.000850},
        {"Ca", 20, 0.000150},
        {"Fe", 26, 0.000010},
        {"Zn", 30, 0.000010}
    };
    return m;
}

BioMaterial BioMaterialLibrary::liver()
{
    BioMaterial m;
    m.id = "liver";
    m.name = "Liver (ICRU-44)";
    m.symbol = "Liver";
    m.category = "biological";
    m.reference = "ICRU Report 44 (1989), Table A.1";
    m.density_g_cm3 = 1.060;
    m.Z_eff = 7.63;
    m.A_eff = 13.17;
    m.I_eV = 75.2;
    m.has_sternheimer = false;
    m.radiation_length_cm = 35.60;
    m.WR = 1.0;
    m.WT = 0.04;
    m.organ_name = "liver";
    m.state = 1;
    m.composition = {
        {"H",  1,  0.101760},
        {"C",  6,  0.139170},
        {"N",  7,  0.030030},
        {"O",  8,  0.716440},
        {"Na", 11, 0.001850},
        {"Mg", 12, 0.000260},
        {"P",  15, 0.002400},
        {"S",  16, 0.003820},
        {"Cl", 17, 0.002320},
        {"K",  19, 0.002770},
        {"Ca", 20, 0.000490},
        {"Fe", 26, 0.000560},
        {"Cu", 29, 0.000060},
        {"Zn", 30, 0.000320}
    };
    return m;
}

BioMaterial BioMaterialLibrary::eyeLens()
{
    BioMaterial m;
    m.id = "eye_lens";
    m.name = "Eye Lens (ICRU-44)";
    m.symbol = "Lens";
    m.category = "biological";
    m.reference = "ICRU Report 44 (1989)";
    m.density_g_cm3 = 1.070;
    m.Z_eff = 7.53;
    m.A_eff = 12.91;
    m.I_eV = 73.3;
    m.has_sternheimer = false;
    m.radiation_length_cm = 35.90;
    m.WR = 1.0;
    m.WT = 0.0;
    m.state = 1;
    m.refraction_index = 1.386;
    m.composition = {
        {"H",  1,  0.096000},
        {"C",  6,  0.195000},
        {"N",  7,  0.057000},
        {"O",  8,  0.646000},
        {"Na", 11, 0.001000},
        {"P",  15, 0.001000},
        {"S",  16, 0.003000},
        {"Cl", 17, 0.001000}
    };
    return m;
}

// ── Phantom materials ─────────────────────────────────────────────────────────

BioMaterial BioMaterialLibrary::pmma()
{
    BioMaterial m;
    m.id = "pmma";
    m.name = "PMMA — Polymethyl Methacrylate (Lucite/Plexiglass)";
    m.symbol = "PMMA";
    m.category = "phantom";
    m.reference = "NIST PSTAR; Sternheimer et al. 1984";
    m.density_g_cm3 = 1.190;
    m.Z_eff = 6.56;
    m.A_eff = 11.16;
    m.I_eV = 74.0;
    m.sternheimer_a = 0.09629;
    m.sternheimer_k = 3.4957;
    m.sternheimer_x0 = 0.1347;
    m.sternheimer_x1 = 2.7145;
    m.sternheimer_C_delta = -3.3297;
    m.sternheimer_delta0 = 0.0;
    m.has_sternheimer = true;
    m.radiation_length_cm = 34.30;
    m.nuclear_int_length_cm = 83.20;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {
        {"H", 1, 0.080538},
        {"C", 6, 0.599848},
        {"O", 8, 0.319614}
    };
    return m;
}

BioMaterial BioMaterialLibrary::polyethylene()
{
    BioMaterial m;
    m.id = "polyethylene";
    m.name = "Polyethylene (CH₂)ₙ";
    m.symbol = "PE";
    m.category = "phantom";
    m.reference = "NIST PSTAR; Sternheimer et al. 1984";
    m.density_g_cm3 = 0.940;
    m.Z_eff = 5.44;
    m.A_eff = 9.40;
    m.I_eV = 57.4;
    m.sternheimer_a = 0.12527;
    m.sternheimer_k = 3.3247;
    m.sternheimer_x0 = 0.1370;
    m.sternheimer_x1 = 2.5775;
    m.sternheimer_C_delta = -3.1986;
    m.sternheimer_delta0 = 0.0;
    m.has_sternheimer = true;
    m.radiation_length_cm = 44.80;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {{"H", 1, 0.143716}, {"C", 6, 0.856284}};
    return m;
}

BioMaterial BioMaterialLibrary::polystyrene()
{
    BioMaterial m;
    m.id = "polystyrene";
    m.name = "Polystyrene (C₈H₈)ₙ";
    m.symbol = "PS";
    m.category = "phantom";
    m.reference = "NIST PSTAR; Sternheimer et al. 1984";
    m.density_g_cm3 = 1.060;
    m.Z_eff = 5.74;
    m.A_eff = 9.97;
    m.I_eV = 68.7;
    m.sternheimer_a = 0.16454;
    m.sternheimer_k = 3.2224;
    m.sternheimer_x0 = 0.1655;
    m.sternheimer_x1 = 2.5014;
    m.sternheimer_C_delta = -3.2999;
    m.has_sternheimer = true;
    m.radiation_length_cm = 42.40;
    m.WR = 1.0;
    m.state = 0;
    m.scintillation_yield_photons_MeV = 10000.0;
    m.composition = {{"H", 1, 0.077418}, {"C", 6, 0.922582}};
    return m;
}

BioMaterial BioMaterialLibrary::graphite()
{
    BioMaterial m;
    m.id = "graphite";
    m.name = "Graphite (Carbon)";
    m.symbol = "C";
    m.category = "phantom";
    m.reference = "NIST PSTAR; Sternheimer et al. 1984";
    m.density_g_cm3 = 2.210;
    m.Z_eff = 6.0;
    m.A_eff = 12.011;
    m.I_eV = 78.0;
    m.sternheimer_a = 0.20762;
    m.sternheimer_k = 2.9532;
    m.sternheimer_x0 = -0.0178;
    m.sternheimer_x1 = 2.4158;
    m.sternheimer_C_delta = -2.8926;
    m.has_sternheimer = true;
    m.radiation_length_cm = 18.80;
    m.nuclear_int_length_cm = 39.40;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {{"C", 6, 1.0}};
    return m;
}

BioMaterial BioMaterialLibrary::solidWaterRW3()
{
    BioMaterial m;
    m.id = "solid_water_rw3";
    m.name = "Solid Water RW3 (PTW)";
    m.symbol = "RW3";
    m.category = "phantom";
    m.reference = "PTW datasheet; White et al. 2008";
    m.density_g_cm3 = 1.045;
    m.Z_eff = 7.39;
    m.A_eff = 12.47;
    m.I_eV = 75.3;
    m.has_sternheimer = false;
    m.radiation_length_cm = 35.80;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {
        {"H", 1, 0.080000}, {"C", 6, 0.672000},
        {"N", 7, 0.024400}, {"O", 8, 0.199600},
        {"Cl",17, 0.017000}, {"Ti",22, 0.007000}
    };
    return m;
}

BioMaterial BioMaterialLibrary::virtualWater()
{
    BioMaterial m;
    m.id = "virtual_water";
    m.name = "Virtual Water (Med-Cal)";
    m.symbol = "VW";
    m.category = "phantom";
    m.reference = "Med-Cal datasheet";
    m.density_g_cm3 = 1.020;
    m.Z_eff = 7.30;
    m.A_eff = 12.37;
    m.I_eV = 75.0;
    m.has_sternheimer = false;
    m.radiation_length_cm = 36.00;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {
        {"H", 1, 0.081000}, {"C", 6, 0.672000},
        {"O", 8, 0.199000}, {"Cl",17, 0.031000},
        {"Ca",20, 0.017000}
    };
    return m;
}

BioMaterial BioMaterialLibrary::A150TissueEquiv()
{
    BioMaterial m;
    m.id = "a150_tissue_equiv";
    m.name = "A-150 Tissue-Equivalent Plastic";
    m.symbol = "A150";
    m.category = "phantom";
    m.reference = "ICRU Report 14 (1969)";
    m.density_g_cm3 = 1.127;
    m.Z_eff = 6.20;
    m.A_eff = 10.92;
    m.I_eV = 65.1;
    m.has_sternheimer = false;
    m.radiation_length_cm = 39.60;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {
        {"H",  1,  0.101327}, {"C",  6,  0.775501},
        {"N",  7,  0.035057}, {"O",  8,  0.052316},
        {"F",  9,  0.017422}, {"Ca", 20, 0.018378}
    };
    return m;
}

BioMaterial BioMaterialLibrary::muscleEquivPhantom()
{
    BioMaterial m = muscleSkeletalICRU();
    m.id = "muscle_equiv_phantom";
    m.name = "Muscle-Equivalent Phantom Material";
    m.category = "phantom";
    m.density_g_cm3 = 1.060;
    return m;
}

BioMaterial BioMaterialLibrary::boneEquivPhantom()
{
    BioMaterial m = boneCorticalICRU();
    m.id = "bone_equiv_phantom";
    m.name = "Bone-Equivalent Phantom Material";
    m.category = "phantom";
    m.density_g_cm3 = 1.820;
    return m;
}

BioMaterial BioMaterialLibrary::hydroxyapatite()
{
    BioMaterial m;
    m.id = "hydroxyapatite";
    m.name = "Hydroxyapatite Ca₅(PO₄)₃OH";
    m.symbol = "HAp";
    m.category = "phantom";
    m.reference = "NIST; Haberland et al.";
    m.density_g_cm3 = 3.160;
    m.Z_eff = 14.05;
    m.A_eff = 22.98;
    m.I_eV = 91.9;
    m.has_sternheimer = false;
    m.radiation_length_cm = 15.10;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {
        {"H",  1,  0.002016}, {"O",  8,  0.410625},
        {"P",  15, 0.184980}, {"Ca", 20, 0.402380}
    };
    return m;
}

BioMaterial BioMaterialLibrary::ptfe()
{
    BioMaterial m;
    m.id = "ptfe";
    m.name = "PTFE — Polytetrafluoroethylene (Teflon)";
    m.symbol = "PTFE";
    m.category = "plastic";
    m.reference = "NIST PSTAR; Sternheimer et al. 1984";
    m.density_g_cm3 = 2.200;
    m.Z_eff = 8.44;
    m.A_eff = 15.52;
    m.I_eV = 99.1;
    m.sternheimer_a = 0.10407;
    m.sternheimer_k = 3.4046;
    m.sternheimer_x0 = 0.2022;
    m.sternheimer_x1 = 2.7071;
    m.sternheimer_C_delta = -3.4046;
    m.has_sternheimer = true;
    m.radiation_length_cm = 15.50;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {{"C", 6, 0.240183}, {"F", 9, 0.759817}};
    return m;
}

// ── Gases ─────────────────────────────────────────────────────────────────────

BioMaterial BioMaterialLibrary::airSTP()
{
    BioMaterial m;
    m.id = "air_stp";
    m.name = "Air — Dry, STP (0°C, 1 atm)";
    m.symbol = "Air";
    m.category = "gas";
    m.reference = "NIST; Sternheimer et al. 1984";
    m.density_g_cm3 = 0.001205;
    m.Z_eff = 7.36;
    m.A_eff = 14.51;
    m.I_eV = 85.7;
    m.sternheimer_a = 0.10914;
    m.sternheimer_k = 3.3994;
    m.sternheimer_x0 = 1.7418;
    m.sternheimer_x1 = 4.2759;
    m.sternheimer_C_delta = -10.5961;
    m.sternheimer_delta0 = 0.0;
    m.has_sternheimer = true;
    m.radiation_length_cm = 30420.0;
    m.nuclear_int_length_cm = 74200.0;
    m.WR = 1.0;
    m.state = 2;
    m.composition = {
        {"C",  6,  0.000124},
        {"N",  7,  0.755267},
        {"O",  8,  0.231781},
        {"Ar", 18, 0.012827}
    };
    return m;
}

BioMaterial BioMaterialLibrary::airAtm20C()
{
    BioMaterial m = airSTP();
    m.id = "air_1atm_20c";
    m.name = "Air — Dry, 1 atm, 20°C";
    m.density_g_cm3 = 0.001204;
    m.radiation_length_cm = 30440.0;
    return m;
}

BioMaterial BioMaterialLibrary::argonGas()
{
    BioMaterial m;
    m.id = "argon_gas";
    m.name = "Argon Gas (STP)";
    m.symbol = "Ar";
    m.category = "gas";
    m.reference = "NIST PSTAR; Sternheimer et al. 1984";
    m.density_g_cm3 = 0.001662;
    m.Z_eff = 18.0;
    m.A_eff = 39.948;
    m.I_eV = 188.0;
    m.sternheimer_a = 0.19714;
    m.sternheimer_k = 2.9618;
    m.sternheimer_x0 = 1.7635;
    m.sternheimer_x1 = 4.4855;
    m.sternheimer_C_delta = -11.9480;
    m.has_sternheimer = true;
    m.radiation_length_cm = 19700.0;
    m.WR = 1.0;
    m.state = 2;
    m.composition = {{"Ar", 18, 1.0}};
    return m;
}

BioMaterial BioMaterialLibrary::heliumGas()
{
    BioMaterial m;
    m.id = "helium_gas";
    m.name = "Helium Gas (STP)";
    m.symbol = "He";
    m.category = "gas";
    m.reference = "NIST PSTAR";
    m.density_g_cm3 = 0.000166;
    m.Z_eff = 2.0;
    m.A_eff = 4.003;
    m.I_eV = 41.8;
    m.has_sternheimer = false;
    m.radiation_length_cm = 754800.0;
    m.WR = 1.0;
    m.state = 2;
    m.composition = {{"He", 2, 1.0}};
    return m;
}

BioMaterial BioMaterialLibrary::co2Gas()
{
    BioMaterial m;
    m.id = "co2_gas";
    m.name = "Carbon Dioxide Gas CO₂ (STP)";
    m.symbol = "CO₂";
    m.category = "gas";
    m.reference = "NIST PSTAR; Sternheimer et al. 1984";
    m.density_g_cm3 = 0.001842;
    m.Z_eff = 7.81;
    m.A_eff = 13.86;
    m.I_eV = 85.0;
    m.has_sternheimer = false;
    m.radiation_length_cm = 18300.0;
    m.WR = 1.0;
    m.state = 2;
    m.composition = {{"C", 6, 0.272916}, {"O", 8, 0.727084}};
    return m;
}

// ── Metals and shielding ──────────────────────────────────────────────────────

BioMaterial BioMaterialLibrary::aluminum()
{
    BioMaterial m;
    m.id = "aluminum";
    m.name = "Aluminum";
    m.symbol = "Al";
    m.category = "metal";
    m.reference = "NIST PSTAR; Sternheimer et al. 1984";
    m.density_g_cm3 = 2.699;
    m.Z_eff = 13.0;
    m.A_eff = 26.982;
    m.I_eV = 166.0;
    m.sternheimer_a = 0.08024;
    m.sternheimer_k = 3.6345;
    m.sternheimer_x0 = 0.1708;
    m.sternheimer_x1 = 3.0127;
    m.sternheimer_C_delta = -4.2395;
    m.has_sternheimer = true;
    m.radiation_length_cm = 8.897;
    m.nuclear_int_length_cm = 38.90;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {{"Al", 13, 1.0}};
    return m;
}

BioMaterial BioMaterialLibrary::iron()
{
    BioMaterial m;
    m.id = "iron";
    m.name = "Iron (Steel)";
    m.symbol = "Fe";
    m.category = "metal";
    m.reference = "NIST PSTAR; Sternheimer et al. 1984";
    m.density_g_cm3 = 7.874;
    m.Z_eff = 26.0;
    m.A_eff = 55.845;
    m.I_eV = 286.0;
    m.sternheimer_a = 0.14680;
    m.sternheimer_k = 2.9632;
    m.sternheimer_x0 = -0.0012;
    m.sternheimer_x1 = 3.1531;
    m.sternheimer_C_delta = -4.2911;
    m.has_sternheimer = true;
    m.radiation_length_cm = 1.757;
    m.nuclear_int_length_cm = 16.77;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {{"Fe", 26, 1.0}};
    return m;
}

BioMaterial BioMaterialLibrary::copper()
{
    BioMaterial m;
    m.id = "copper";
    m.name = "Copper";
    m.symbol = "Cu";
    m.category = "metal";
    m.reference = "NIST PSTAR; Sternheimer et al. 1984";
    m.density_g_cm3 = 8.960;
    m.Z_eff = 29.0;
    m.A_eff = 63.546;
    m.I_eV = 322.0;
    m.sternheimer_a = 0.14339;
    m.sternheimer_k = 2.9044;
    m.sternheimer_x0 = -0.0254;
    m.sternheimer_x1 = 3.2792;
    m.sternheimer_C_delta = -4.4190;
    m.has_sternheimer = true;
    m.radiation_length_cm = 1.436;
    m.nuclear_int_length_cm = 15.32;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {{"Cu", 29, 1.0}};
    return m;
}

BioMaterial BioMaterialLibrary::lead()
{
    BioMaterial m;
    m.id = "lead";
    m.name = "Lead";
    m.symbol = "Pb";
    m.category = "shielding";
    m.reference = "NIST PSTAR; Sternheimer et al. 1984";
    m.density_g_cm3 = 11.350;
    m.Z_eff = 82.0;
    m.A_eff = 207.2;
    m.I_eV = 823.0;
    m.sternheimer_a = 0.09359;
    m.sternheimer_k = 3.1608;
    m.sternheimer_x0 = 0.3776;
    m.sternheimer_x1 = 3.8073;
    m.sternheimer_C_delta = -6.2018;
    m.has_sternheimer = true;
    m.radiation_length_cm = 0.5612;
    m.nuclear_int_length_cm = 17.09;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {{"Pb", 82, 1.0}};
    return m;
}

BioMaterial BioMaterialLibrary::tungsten()
{
    BioMaterial m;
    m.id = "tungsten";
    m.name = "Tungsten";
    m.symbol = "W";
    m.category = "shielding";
    m.reference = "NIST PSTAR; Sternheimer et al. 1984";
    m.density_g_cm3 = 19.300;
    m.Z_eff = 74.0;
    m.A_eff = 183.84;
    m.I_eV = 727.0;
    m.has_sternheimer = false;
    m.radiation_length_cm = 0.3504;
    m.nuclear_int_length_cm = 9.947;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {{"W", 74, 1.0}};
    return m;
}

BioMaterial BioMaterialLibrary::gold()
{
    BioMaterial m;
    m.id = "gold";
    m.name = "Gold";
    m.symbol = "Au";
    m.category = "metal";
    m.reference = "NIST PSTAR";
    m.density_g_cm3 = 19.320;
    m.Z_eff = 79.0;
    m.A_eff = 196.97;
    m.I_eV = 790.0;
    m.has_sternheimer = false;
    m.radiation_length_cm = 0.3344;
    m.nuclear_int_length_cm = 10.15;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {{"Au", 79, 1.0}};
    return m;
}

BioMaterial BioMaterialLibrary::titanium()
{
    BioMaterial m;
    m.id = "titanium";
    m.name = "Titanium";
    m.symbol = "Ti";
    m.category = "metal";
    m.reference = "NIST PSTAR";
    m.density_g_cm3 = 4.540;
    m.Z_eff = 22.0;
    m.A_eff = 47.867;
    m.I_eV = 233.0;
    m.has_sternheimer = false;
    m.radiation_length_cm = 3.560;
    m.nuclear_int_length_cm = 27.40;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {{"Ti", 22, 1.0}};
    return m;
}

BioMaterial BioMaterialLibrary::stainlessSteel304()
{
    BioMaterial m;
    m.id = "stainless_steel_304";
    m.name = "Stainless Steel 304";
    m.symbol = "SS304";
    m.category = "metal";
    m.reference = "ASTM A240/A240M";
    m.density_g_cm3 = 8.000;
    m.Z_eff = 25.72;
    m.A_eff = 54.70;
    m.I_eV = 282.0;
    m.has_sternheimer = false;
    m.radiation_length_cm = 1.730;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {
        {"C",  6,  0.000400}, {"Si", 14, 0.010000},
        {"Mn", 25, 0.020000}, {"Cr", 24, 0.190000},
        {"Ni", 28, 0.100000}, {"Fe", 26, 0.679600}
    };
    return m;
}

BioMaterial BioMaterialLibrary::beryllium()
{
    BioMaterial m;
    m.id = "beryllium";
    m.name = "Beryllium";
    m.symbol = "Be";
    m.category = "metal";
    m.reference = "NIST PSTAR; Sternheimer et al. 1984";
    m.density_g_cm3 = 1.848;
    m.Z_eff = 4.0;
    m.A_eff = 9.012;
    m.I_eV = 63.7;
    m.sternheimer_a = 0.80392;
    m.sternheimer_k = 2.4339;
    m.sternheimer_x0 = 0.0592;
    m.sternheimer_x1 = 1.6922;
    m.sternheimer_C_delta = -2.7847;
    m.has_sternheimer = true;
    m.radiation_length_cm = 35.28;
    m.nuclear_int_length_cm = 39.40;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {{"Be", 4, 1.0}};
    return m;
}

BioMaterial BioMaterialLibrary::tantalum()
{
    BioMaterial m;
    m.id = "tantalum";
    m.name = "Tantalum";
    m.symbol = "Ta";
    m.category = "shielding";
    m.reference = "NIST PSTAR";
    m.density_g_cm3 = 16.650;
    m.Z_eff = 73.0;
    m.A_eff = 180.95;
    m.I_eV = 718.0;
    m.has_sternheimer = false;
    m.radiation_length_cm = 0.4094;
    m.nuclear_int_length_cm = 11.08;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {{"Ta", 73, 1.0}};
    return m;
}

BioMaterial BioMaterialLibrary::uranium()
{
    BioMaterial m;
    m.id = "uranium";
    m.name = "Uranium (depleted)";
    m.symbol = "U";
    m.category = "shielding";
    m.reference = "NIST PSTAR";
    m.density_g_cm3 = 18.950;
    m.Z_eff = 92.0;
    m.A_eff = 238.03;
    m.I_eV = 890.0;
    m.has_sternheimer = false;
    m.radiation_length_cm = 0.3166;
    m.nuclear_int_length_cm = 11.89;
    m.WR = 20.0;
    m.state = 0;
    m.composition = {{"U", 92, 1.0}};
    return m;
}

BioMaterial BioMaterialLibrary::bismuth()
{
    BioMaterial m;
    m.id = "bismuth";
    m.name = "Bismuth";
    m.symbol = "Bi";
    m.category = "shielding";
    m.reference = "NIST PSTAR";
    m.density_g_cm3 = 9.747;
    m.Z_eff = 83.0;
    m.A_eff = 208.98;
    m.I_eV = 823.0;
    m.has_sternheimer = false;
    m.radiation_length_cm = 0.5526;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {{"Bi", 83, 1.0}};
    return m;
}

// ── Plastics and composites ───────────────────────────────────────────────────

BioMaterial BioMaterialLibrary::kapton()
{
    BioMaterial m;
    m.id = "kapton";
    m.name = "Kapton — Polyimide Film";
    m.symbol = "Kapton";
    m.category = "plastic";
    m.reference = "DuPont datasheet; NIST PSTAR";
    m.density_g_cm3 = 1.420;
    m.Z_eff = 6.69;
    m.A_eff = 11.73;
    m.I_eV = 79.6;
    m.has_sternheimer = false;
    m.radiation_length_cm = 28.60;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {
        {"H", 1, 0.026362}, {"C", 6, 0.691133},
        {"N", 7, 0.073270}, {"O", 8, 0.209235}
    };
    return m;
}

BioMaterial BioMaterialLibrary::mylar()
{
    BioMaterial m;
    m.id = "mylar";
    m.name = "Mylar — Biaxially Oriented PET";
    m.symbol = "PET";
    m.category = "plastic";
    m.reference = "NIST PSTAR; Sternheimer et al. 1984";
    m.density_g_cm3 = 1.400;
    m.Z_eff = 6.50;
    m.A_eff = 11.10;
    m.I_eV = 78.7;
    m.sternheimer_a = 0.13095;
    m.sternheimer_k = 3.2815;
    m.sternheimer_x0 = 0.1562;
    m.sternheimer_x1 = 2.6507;
    m.sternheimer_C_delta = -3.3262;
    m.has_sternheimer = true;
    m.radiation_length_cm = 28.50;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {
        {"H", 1, 0.041959}, {"C", 6, 0.625017}, {"O", 8, 0.333025}
    };
    return m;
}

BioMaterial BioMaterialLibrary::carbonFiber()
{
    BioMaterial m;
    m.id = "carbon_fiber_cfrp";
    m.name = "Carbon Fiber Reinforced Polymer (CFRP)";
    m.symbol = "CFRP";
    m.category = "plastic";
    m.reference = "Composite average";
    m.density_g_cm3 = 1.580;
    m.Z_eff = 6.10;
    m.A_eff = 10.80;
    m.I_eV = 78.0;
    m.has_sternheimer = false;
    m.radiation_length_cm = 22.00;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {
        {"H", 1, 0.020000}, {"C", 6, 0.900000}, {"O", 8, 0.080000}
    };
    return m;
}

BioMaterial BioMaterialLibrary::boratedPolyethylene()
{
    BioMaterial m;
    m.id = "borated_pe";
    m.name = "Borated Polyethylene (5% B)";
    m.symbol = "BPE";
    m.category = "shielding";
    m.reference = "Standard neutron shielding material";
    m.density_g_cm3 = 0.970;
    m.Z_eff = 5.48;
    m.A_eff = 9.47;
    m.I_eV = 57.4;
    m.has_sternheimer = false;
    m.radiation_length_cm = 43.70;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {
        {"H",  1,  0.136000}, {"B",  5,  0.050000},
        {"C",  6,  0.814000}
    };
    return m;
}

BioMaterial BioMaterialLibrary::paraffin()
{
    BioMaterial m;
    m.id = "paraffin";
    m.name = "Paraffin Wax (CₙH₂ₙ₊₂)";
    m.symbol = "Wax";
    m.category = "phantom";
    m.reference = "NIST PSTAR";
    m.density_g_cm3 = 0.930;
    m.Z_eff = 5.53;
    m.A_eff = 9.56;
    m.I_eV = 55.9;
    m.has_sternheimer = false;
    m.radiation_length_cm = 44.80;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {{"H", 1, 0.148605}, {"C", 6, 0.851395}};
    return m;
}

BioMaterial BioMaterialLibrary::concrete()
{
    BioMaterial m;
    m.id = "concrete_standard";
    m.name = "Concrete (Ordinary, Portland)";
    m.symbol = "Conc";
    m.category = "shielding";
    m.reference = "NIST PSTAR; Sternheimer et al. 1984";
    m.density_g_cm3 = 2.300;
    m.Z_eff = 11.33;
    m.A_eff = 21.82;
    m.I_eV = 135.2;
    m.has_sternheimer = false;
    m.radiation_length_cm = 11.55;
    m.nuclear_int_length_cm = 40.04;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {
        {"H",  1,  0.010000}, {"C",  6,  0.001000},
        {"O",  8,  0.529107}, {"Na", 11, 0.016000},
        {"Mg", 12, 0.002000}, {"Al", 13, 0.033872},
        {"Si", 14, 0.337021}, {"K",  19, 0.013000},
        {"Ca", 20, 0.044000}, {"Fe", 26, 0.014000}
    };
    return m;
}

BioMaterial BioMaterialLibrary::waterEquivCement()
{
    BioMaterial m;
    m.id = "water_equiv_cement";
    m.name = "Water-Equivalent Cement";
    m.symbol = "WEC";
    m.category = "phantom";
    m.reference = "Moyers & Vatnitsky 2012";
    m.density_g_cm3 = 1.030;
    m.Z_eff = 7.30;
    m.A_eff = 12.35;
    m.I_eV = 75.3;
    m.has_sternheimer = false;
    m.radiation_length_cm = 35.90;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {
        {"H", 1, 0.096800}, {"C", 6, 0.143900},
        {"N", 7, 0.017800}, {"O", 8, 0.722900},
        {"Ca",20, 0.018600}
    };
    return m;
}

// ── Detector crystals ─────────────────────────────────────────────────────────

BioMaterial BioMaterialLibrary::silicon()
{
    BioMaterial m;
    m.id = "silicon";
    m.name = "Silicon (detector-grade)";
    m.symbol = "Si";
    m.category = "detector";
    m.reference = "NIST PSTAR; Sternheimer et al. 1984";
    m.density_g_cm3 = 2.330;
    m.Z_eff = 14.0;
    m.A_eff = 28.086;
    m.I_eV = 173.0;
    m.sternheimer_a = 0.14921;
    m.sternheimer_k = 3.2546;
    m.sternheimer_x0 = 0.2015;
    m.sternheimer_x1 = 2.8716;
    m.sternheimer_C_delta = -4.4355;
    m.has_sternheimer = true;
    m.radiation_length_cm = 9.370;
    m.nuclear_int_length_cm = 46.52;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {{"Si", 14, 1.0}};
    return m;
}

BioMaterial BioMaterialLibrary::germanium()
{
    BioMaterial m;
    m.id = "germanium";
    m.name = "Germanium HPGe";
    m.symbol = "Ge";
    m.category = "detector";
    m.reference = "NIST PSTAR";
    m.density_g_cm3 = 5.323;
    m.Z_eff = 32.0;
    m.A_eff = 72.630;
    m.I_eV = 350.0;
    m.has_sternheimer = false;
    m.radiation_length_cm = 2.301;
    m.nuclear_int_length_cm = 26.84;
    m.WR = 1.0;
    m.state = 0;
    m.composition = {{"Ge", 32, 1.0}};
    return m;
}

BioMaterial BioMaterialLibrary::naiCrystal()
{
    BioMaterial m;
    m.id = "nai_crystal";
    m.name = "NaI(Tl) Scintillator Crystal";
    m.symbol = "NaI";
    m.category = "detector";
    m.reference = "Saint-Gobain datasheet; NIST";
    m.density_g_cm3 = 3.670;
    m.Z_eff = 32.03;
    m.A_eff = 73.45;
    m.I_eV = 452.0;
    m.has_sternheimer = false;
    m.radiation_length_cm = 2.590;
    m.WR = 1.0;
    m.state = 0;
    m.scintillation_yield_photons_MeV = 38000.0;
    m.refraction_index = 1.775;
    m.composition = {{"Na", 11, 0.153370}, {"I", 53, 0.846630}};
    return m;
}

BioMaterial BioMaterialLibrary::bgoCrystal()
{
    BioMaterial m;
    m.id = "bgo_crystal";
    m.name = "BGO — Bismuth Germanate Bi₄Ge₃O₁₂";
    m.symbol = "BGO";
    m.category = "detector";
    m.reference = "Saint-Gobain datasheet; NIST";
    m.density_g_cm3 = 7.130;
    m.Z_eff = 74.22;
    m.A_eff = 199.58;
    m.I_eV = 534.1;
    m.has_sternheimer = false;
    m.radiation_length_cm = 1.120;
    m.WR = 1.0;
    m.state = 0;
    m.scintillation_yield_photons_MeV = 8200.0;
    m.refraction_index = 2.150;
    m.composition = {
        {"O",  8,  0.154126}, {"Ge", 32, 0.178508}, {"Bi", 83, 0.667366}
    };
    return m;
}

BioMaterial BioMaterialLibrary::lysoCrystal()
{
    BioMaterial m;
    m.id = "lyso_crystal";
    m.name = "LYSO — Lu₂(1₋ˣ)Y₂ˣSiO₅";
    m.symbol = "LYSO";
    m.category = "detector";
    m.reference = "Saint-Gobain datasheet";
    m.density_g_cm3 = 7.100;
    m.Z_eff = 65.51;
    m.A_eff = 165.20;
    m.I_eV = 694.0;
    m.has_sternheimer = false;
    m.radiation_length_cm = 1.140;
    m.WR = 1.0;
    m.state = 0;
    m.scintillation_yield_photons_MeV = 32000.0;
    m.refraction_index = 1.810;
    m.composition = {
        {"O",  8,  0.181600}, {"Si", 14, 0.063700},
        {"Y",  39, 0.019100}, {"Lu", 71, 0.735600}
    };
    return m;
}

BioMaterial BioMaterialLibrary::csiCrystal()
{
    BioMaterial m;
    m.id = "csi_crystal";
    m.name = "CsI(Tl) Scintillator Crystal";
    m.symbol = "CsI";
    m.category = "detector";
    m.reference = "Saint-Gobain datasheet; NIST";
    m.density_g_cm3 = 4.510;
    m.Z_eff = 54.09;
    m.A_eff = 129.91;
    m.I_eV = 553.1;
    m.has_sternheimer = false;
    m.radiation_length_cm = 1.860;
    m.WR = 1.0;
    m.state = 0;
    m.scintillation_yield_photons_MeV = 54000.0;
    m.refraction_index = 1.788;
    m.composition = {{"Cs", 55, 0.511550}, {"I", 53, 0.488450}};
    return m;
}

// ── Library constructor ───────────────────────────────────────────────────────

BioMaterialLibrary::BioMaterialLibrary()
{
    materials_ = {
        // Biological (ICRU-44)
        water(), muscleSkeletalICRU(), boneCorticalICRU(), boneSpongy(),
        adiposeTissue(), brainGrey(), brainWhite(),
        lungInflated(), lungDeflated(), blood(), skin(), liver(), eyeLens(),
        // Phantom
        pmma(), polyethylene(), polystyrene(), graphite(),
        solidWaterRW3(), virtualWater(), A150TissueEquiv(),
        muscleEquivPhantom(), boneEquivPhantom(),
        hydroxyapatite(), ptfe(),
        // Gases
        airSTP(), airAtm20C(), argonGas(), heliumGas(), co2Gas(),
        // Metals
        aluminum(), iron(), copper(), lead(), tungsten(), gold(),
        titanium(), stainlessSteel304(), beryllium(), tantalum(),
        uranium(), bismuth(),
        // Plastics and composites
        kapton(), mylar(), carbonFiber(), boratedPolyethylene(),
        paraffin(), concrete(), waterEquivCement(),
        // Detectors
        silicon(), germanium(), naiCrystal(), bgoCrystal(),
        lysoCrystal(), csiCrystal()
    };
}

// ── Queries ───────────────────────────────────────────────────────────────────

const std::vector<BioMaterial>& BioMaterialLibrary::all() const
{
    return materials_;
}

std::optional<BioMaterial> BioMaterialLibrary::findById(const std::string& id) const
{
    const auto target = toLower(id);
    for (const auto& m : materials_) {
        if (toLower(m.id) == target) {
            return m;
        }
    }
    return std::nullopt;
}

std::optional<BioMaterial> BioMaterialLibrary::findByName(const std::string& query) const
{
    const auto target = toLower(query);
    for (const auto& m : materials_) {
        if (toLower(m.name).find(target) != std::string::npos ||
            toLower(m.symbol).find(target) != std::string::npos) {
            return m;
        }
    }
    return std::nullopt;
}

std::vector<BioMaterial> BioMaterialLibrary::byCategory(const std::string& category) const
{
    const auto target = toLower(category);
    std::vector<BioMaterial> result;
    for (const auto& m : materials_) {
        if (toLower(m.category) == target) {
            result.push_back(m);
        }
    }
    return result;
}

void BioMaterialLibrary::addCustom(const BioMaterial& mat)
{
    for (auto& m : materials_) {
        if (m.id == mat.id) {
            m = mat;
            return;
        }
    }
    materials_.push_back(mat);
}

void BioMaterialLibrary::removeCustom(const std::string& id)
{
    materials_.erase(
        std::remove_if(materials_.begin(), materials_.end(),
            [&](const BioMaterial& m) { return m.id == id && m.is_custom; }),
        materials_.end());
}

} // namespace beamlab::biosim
