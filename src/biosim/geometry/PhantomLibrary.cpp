#include "biosim/geometry/PhantomLibrary.h"
#include "biosim/materials/BioMaterialLibrary.h"

#include <cctype>

namespace beamlab::biosim {

namespace {
std::string toLower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Helper: build a simple slab at a given axial start position.
// The beam axis is Z in the real data (axial ~11.55 m), but phantoms are defined
// relative to the beam entry, so we use offset from 0.0.
BioSlab makeSlab(const std::string& id,
                  const std::string& label,
                  const BioMaterial& mat,
                  double start_m,
                  double thick_m,
                  unsigned int color_rgba = 0x64B4FF78)
{
    BioSlab s;
    s.id             = id;
    s.label          = label;
    s.material       = mat;
    s.axial_start_m  = start_m;
    s.thickness_m    = thick_m;
    s.shape          = BioSlab::Shape::Infinite;
    s.color_rgba     = color_rgba;
    s.enabled        = true;
    return s;
}
} // namespace

// ── Phantom definitions ───────────────────────────────────────────────────────

PhantomPreset PhantomLibrary::iaeaWater30()
{
    BioMaterialLibrary lib;
    const auto water = lib.water();
    PhantomPreset p;
    p.id          = "iaea_water_30x30x30";
    p.name        = "IAEA Water Phantom 30×30×30 cm";
    p.category    = "IAEA";
    p.description = "Standard 30×30×30 cm³ water phantom used as absolute reference "
                    "for dosimetry and range verification. IAEA TRS-398.";
    p.reference   = "IAEA TRS-398 (2000); IAEA TECDOC-1637 (2012)";
    p.slabs = { makeSlab("water_30cm", "Water (30 cm)", water, 0.0, 0.30, 0x2080FF78) };
    return p;
}

PhantomPreset PhantomLibrary::icruHead()
{
    BioMaterialLibrary lib;
    PhantomPreset p;
    p.id       = "icru_head";
    p.name     = "ICRU Head Phantom";
    p.category = "ICRU";
    p.description = "Layered head phantom: scalp skin + adipose + skull bone "
                    "+ dura mater (muscle equiv) + brain grey matter. "
                    "Used for cranial irradiation planning.";
    p.reference = "ICRU Report 48 (1992); White et al., PMB 1987";
    double z = 0.0;
    p.slabs = {
        makeSlab("scalp",   "Skin (scalp, 5 mm)",   lib.skin(),             z,       0.005, 0xF4C24278),
        makeSlab("fat",     "Adipose (3 mm)",        lib.adiposeTissue(),    z+=0.005,0.003, 0xFFD98A78),
        makeSlab("skull",   "Cortical Bone (7 mm)",  lib.boneCorticalICRU(), z+=0.003,0.007, 0xE8E0C878),
        makeSlab("dura",    "Muscle — dura equiv (2 mm)", lib.muscleSkeletalICRU(), z+=0.007, 0.002, 0xC87A7A78),
        makeSlab("brain",   "Brain grey matter (120 mm)", lib.brainGrey(),   z+=0.002, 0.120, 0x90B8E878),
    };
    return p;
}

PhantomPreset PhantomLibrary::icruTrunk()
{
    BioMaterialLibrary lib;
    PhantomPreset p;
    p.id       = "icru_trunk";
    p.name     = "ICRU Trunk Phantom";
    p.category = "ICRU";
    p.description = "7-layer trunk phantom: skin + adipose + muscle + rib bone "
                    "+ lung + muscle + skin. Representative of thorax geometry "
                    "for lung and chest treatment planning.";
    p.reference = "ICRU Report 48 (1992)";
    double z = 0.0;
    p.slabs = {
        makeSlab("skin_ant",  "Skin anterior (3 mm)",  lib.skin(),             z,        0.003, 0xF4C24278),
        makeSlab("fat_ant",   "Adipose (20 mm)",        lib.adiposeTissue(),    z+=0.003, 0.020, 0xFFD98A78),
        makeSlab("muscle_ant","Muscle anterior (15 mm)",lib.muscleSkeletalICRU(),z+=0.020,0.015, 0xC87A7A78),
        makeSlab("rib",       "Cortical Bone rib (5 mm)", lib.boneCorticalICRU(),z+=0.015,0.005, 0xE8E0C878),
        makeSlab("lung",      "Lung inflated (140 mm)", lib.lungInflated(),     z+=0.005, 0.140, 0x78C8F478),
        makeSlab("muscle_pos","Muscle posterior (20 mm)",lib.muscleSkeletalICRU(),z+=0.140,0.020, 0xC87A7A78),
        makeSlab("skin_pos",  "Skin posterior (3 mm)", lib.skin(),             z+=0.020, 0.003, 0xF4C24278),
    };
    return p;
}

PhantomPreset PhantomLibrary::protonTherapyHead()
{
    BioMaterialLibrary lib;
    PhantomPreset p;
    p.id       = "proton_therapy_head";
    p.name     = "Proton Therapy — Head Treatment Field";
    p.category = "Clinical";
    p.description = "Clinical head phantom for proton therapy planning: scalp + "
                    "cranial bone + CSF (water equiv) + brain target. "
                    "Optimised for Bragg peak placement in brain tumors.";
    p.reference = "Paganetti 2012, PMB; Mohan & Mahajan 2013";
    double z = 0.0;
    p.slabs = {
        makeSlab("scalp2",   "Skin + fat (8 mm)",    lib.adiposeTissue(),    z,        0.008, 0xF4C24278),
        makeSlab("skull2",   "Skull (compact, 8 mm)",lib.boneCorticalICRU(), z+=0.008, 0.008, 0xE8E0C878),
        makeSlab("csf",      "CSF (water equiv, 5 mm)", lib.water(),         z+=0.008, 0.005, 0x2080FF78),
        makeSlab("brain_target","Brain (tumor target, 40 mm)", lib.brainGrey(), z+=0.005, 0.040, 0x90B8E878),
        makeSlab("brain_distal","Brain (distal, 60 mm)", lib.brainWhite(), z+=0.040, 0.060, 0x78A8D878),
    };
    return p;
}

PhantomPreset PhantomLibrary::protonTherapyProstate()
{
    BioMaterialLibrary lib;
    PhantomPreset p;
    p.id       = "proton_therapy_prostate";
    p.name     = "Proton Therapy — Prostate Treatment Field";
    p.category = "Clinical";
    p.description = "Lateral prostate field: skin + fat + muscle + rectum/bladder "
                    "(water equiv) + prostate target. For low-energy proton beams.";
    p.reference = "Zietman et al. 2010; Shipley et al. JAMA 2017";
    double z = 0.0;
    p.slabs = {
        makeSlab("skin_p",    "Skin (5 mm)",           lib.skin(),             z,        0.005, 0xF4C24278),
        makeSlab("fat_p",     "Adipose (30 mm)",        lib.adiposeTissue(),    z+=0.005, 0.030, 0xFFD98A78),
        makeSlab("muscle_p",  "Muscle (50 mm)",         lib.muscleSkeletalICRU(),z+=0.030,0.050, 0xC87A7A78),
        makeSlab("bladder",   "Bladder/Rectum wall (water, 10 mm)", lib.water(), z+=0.050, 0.010, 0x2080FF78),
        makeSlab("prostate",  "Prostate (target, 35 mm)", lib.muscleSkeletalICRU(), z+=0.010, 0.035, 0xFF6464A0),
    };
    return p;
}

PhantomPreset PhantomLibrary::randoHead()
{
    BioMaterialLibrary lib;
    PhantomPreset p;
    p.id       = "rando_head";
    p.name     = "RANDO Head Phantom (simplified)";
    p.category = "Clinical";
    p.description = "Simplified version of the Alderson RANDO anthropomorphic head "
                    "phantom for clinical dosimetry measurements.";
    p.reference = "Alderson et al. 1962; White 1978, PMB";
    double z = 0.0;
    p.slabs = {
        makeSlab("rando_outer","Outer shell (PMMA equiv, 10 mm)", lib.pmma(), z, 0.010, 0xD4D4D478),
        makeSlab("rando_core", "Core (muscle equiv, 80 mm)", lib.muscleEquivPhantom(), z+=0.010, 0.080, 0xC87A7A78),
        makeSlab("rando_inner","Inner cavity (air, 20 mm)", lib.airSTP(), z+=0.080, 0.020, 0x78D4F478),
    };
    return p;
}

PhantomPreset PhantomLibrary::generic3Layer()
{
    BioMaterialLibrary lib;
    PhantomPreset p;
    p.id       = "generic_3layer";
    p.name     = "Generic 3-Layer Phantom";
    p.category = "Experimental";
    p.description = "Configurable 3-layer phantom: water | muscle | bone. "
                    "Use the Slab Editor to adjust thickness and materials.";
    p.reference = "";
    double z = 0.0;
    p.slabs = {
        makeSlab("g3_1","Water (50 mm)",         lib.water(),            z,        0.050, 0x2080FF78),
        makeSlab("g3_2","Muscle (30 mm)",        lib.muscleSkeletalICRU(),z+=0.050,0.030, 0xC87A7A78),
        makeSlab("g3_3","Cortical Bone (20 mm)", lib.boneCorticalICRU(), z+=0.030, 0.020, 0xE8E0C878),
    };
    return p;
}

PhantomPreset PhantomLibrary::generic5Layer()
{
    BioMaterialLibrary lib;
    PhantomPreset p;
    p.id       = "generic_5layer";
    p.name     = "Generic 5-Layer Phantom";
    p.category = "Experimental";
    p.description = "Configurable 5-layer phantom with varied materials. "
                    "Adjust in the Slab Editor for your experiment.";
    p.reference = "";
    double z = 0.0;
    p.slabs = {
        makeSlab("g5_1","PMMA (5 mm)",           lib.pmma(),             z,        0.005, 0xD4D4D478),
        makeSlab("g5_2","Water (30 mm)",         lib.water(),            z+=0.005, 0.030, 0x2080FF78),
        makeSlab("g5_3","Muscle (20 mm)",        lib.muscleSkeletalICRU(),z+=0.030,0.020, 0xC87A7A78),
        makeSlab("g5_4","Cortical Bone (10 mm)", lib.boneCorticalICRU(), z+=0.020, 0.010, 0xE8E0C878),
        makeSlab("g5_5","Water (20 mm)",         lib.water(),            z+=0.010, 0.020, 0x2080FF78),
    };
    return p;
}

PhantomPreset PhantomLibrary::detectorStack()
{
    BioMaterialLibrary lib;
    PhantomPreset p;
    p.id       = "detector_stack";
    p.name     = "Detector Stack (Si/Al/PMMA/Air)";
    p.category = "Experimental";
    p.description = "Typical detector stack for particle tracking: silicon sensor "
                    "+ aluminum support + PMMA substrate + air gap.";
    p.reference = "Generic tracking detector geometry";
    double z = 0.0;
    p.slabs = {
        makeSlab("si_sensor",  "Silicon sensor (0.3 mm)",  lib.silicon(),  z,        0.0003, 0x60A0FF78),
        makeSlab("al_support", "Aluminum frame (1 mm)",    lib.aluminum(), z+=0.0003,0.001,  0xC0C0C878),
        makeSlab("pmma_sub",   "PMMA substrate (3 mm)",    lib.pmma(),     z+=0.001, 0.003,  0xD4D4D478),
        makeSlab("air_gap",    "Air gap (5 mm)",           lib.airSTP(),   z+=0.003, 0.005,  0x78D4F478),
    };
    return p;
}

PhantomPreset PhantomLibrary::shieldingTest()
{
    BioMaterialLibrary lib;
    PhantomPreset p;
    p.id       = "shielding_test";
    p.name     = "Shielding Test (Pb/Fe/Concrete)";
    p.category = "Experimental";
    p.description = "Multi-layer shielding stack: lead + iron + concrete. "
                    "Useful for testing high-Z material stopping power.";
    p.reference = "";
    double z = 0.0;
    p.slabs = {
        makeSlab("lead_s",     "Lead (50 mm)",           lib.lead(),     z,        0.050, 0x808080A0),
        makeSlab("iron_s",     "Iron (100 mm)",          lib.iron(),     z+=0.050, 0.100, 0x906050A0),
        makeSlab("concrete_s", "Concrete (200 mm)",      lib.concrete(), z+=0.100, 0.200, 0xB0A09078),
    };
    return p;
}

PhantomPreset PhantomLibrary::muscleBone()
{
    BioMaterialLibrary lib;
    PhantomPreset p;
    p.id       = "muscle_bone";
    p.name     = "Muscle + Bone (simple clinical)";
    p.category = "Clinical";
    p.description = "Two-layer phantom for simple beam-range verification: "
                    "muscle tissue followed by cortical bone.";
    p.reference = "";
    double z = 0.0;
    p.slabs = {
        makeSlab("muscle_mb", "Muscle (80 mm)",       lib.muscleSkeletalICRU(), z,        0.080, 0xC87A7A78),
        makeSlab("bone_mb",   "Cortical Bone (20 mm)", lib.boneCorticalICRU(),  z+=0.080, 0.020, 0xE8E0C878),
    };
    return p;
}

PhantomPreset PhantomLibrary::lungPassage()
{
    BioMaterialLibrary lib;
    PhantomPreset p;
    p.id       = "lung_passage";
    p.name     = "Lung Passage (Muscle–Lung–Muscle)";
    p.category = "Clinical";
    p.description = "Three-layer phantom simulating a beam passing through "
                    "chest wall, inflated lung, and chest wall on exit. "
                    "Critical for SBRT/SABR treatment planning.";
    p.reference = "Korreman et al. 2010, Radiother.Oncol.";
    double z = 0.0;
    p.slabs = {
        makeSlab("chest_ant", "Chest wall anterior (20 mm)", lib.muscleSkeletalICRU(), z,        0.020, 0xC87A7A78),
        makeSlab("lung_lp",   "Lung inflated (150 mm)",       lib.lungInflated(),       z+=0.020, 0.150, 0x78C8F478),
        makeSlab("chest_pos", "Chest wall posterior (20 mm)", lib.muscleSkeletalICRU(), z+=0.150, 0.020, 0xC87A7A78),
    };
    return p;
}

PhantomPreset PhantomLibrary::brainTreatment()
{
    BioMaterialLibrary lib;
    PhantomPreset p;
    p.id       = "brain_treatment";
    p.name     = "Brain Treatment (Skull + Brain)";
    p.category = "Clinical";
    p.description = "Four-layer phantom for cranial radiosurgery: scalp skin, "
                    "cortical bone skull, brain grey matter, brain white matter.";
    p.reference = "Ding et al. 2016, Med.Phys.";
    double z = 0.0;
    p.slabs = {
        makeSlab("scalp_bt",  "Scalp (5 mm)",           lib.skin(),             z,        0.005, 0xF4C24278),
        makeSlab("skull_bt",  "Skull bone (7 mm)",      lib.boneCorticalICRU(), z+=0.005, 0.007, 0xE8E0C878),
        makeSlab("brain_gm",  "Brain grey (50 mm)",     lib.brainGrey(),        z+=0.007, 0.050, 0x90B8E878),
        makeSlab("brain_wm",  "Brain white (80 mm)",    lib.brainWhite(),       z+=0.050, 0.080, 0x78A8D878),
    };
    return p;
}

PhantomPreset PhantomLibrary::waterAirInterface()
{
    BioMaterialLibrary lib;
    PhantomPreset p;
    p.id       = "water_air_interface";
    p.name     = "Water–Air Interface (Calibration)";
    p.category = "Experimental";
    p.description = "Water block followed by an air gap. "
                    "Used to calibrate range and dose at material boundaries.";
    p.reference = "";
    double z = 0.0;
    p.slabs = {
        makeSlab("water_wai", "Water (100 mm)",  lib.water(),  z,        0.100, 0x2080FF78),
        makeSlab("air_wai",   "Air (50 mm)",     lib.airSTP(), z+=0.100, 0.050, 0x78D4F478),
    };
    return p;
}

PhantomPreset PhantomLibrary::vacuum()
{
    PhantomPreset p;
    p.id       = "vacuum";
    p.name     = "Vacuum (No Material)";
    p.category = "Experimental";
    p.description = "Empty phantom — no slabs. The beam propagates without "
                    "any material interaction. Use as a reference baseline.";
    p.reference = "";
    p.slabs    = {};
    return p;
}

// ── Library ───────────────────────────────────────────────────────────────────

PhantomLibrary::PhantomLibrary()
{
    phantoms_ = {
        iaeaWater30(), icruHead(), icruTrunk(),
        protonTherapyHead(), protonTherapyProstate(),
        randoHead(), generic3Layer(), generic5Layer(),
        detectorStack(), shieldingTest(),
        muscleBone(), lungPassage(), brainTreatment(),
        waterAirInterface(), vacuum()
    };
}

const std::vector<PhantomPreset>& PhantomLibrary::all() const { return phantoms_; }

std::optional<PhantomPreset> PhantomLibrary::findById(const std::string& id) const
{
    const auto t = toLower(id);
    for (const auto& p : phantoms_) {
        if (toLower(p.id) == t) return p;
    }
    return std::nullopt;
}

std::vector<PhantomPreset> PhantomLibrary::byCategory(const std::string& cat) const
{
    const auto t = toLower(cat);
    std::vector<PhantomPreset> out;
    for (const auto& p : phantoms_) {
        if (toLower(p.category) == t) out.push_back(p);
    }
    return out;
}

} // namespace beamlab::biosim
