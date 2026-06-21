#include "ui/views/ValidationView.h"
#include "ui/validation/NistValidator.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <cmath>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

namespace beamlab::ui {

namespace {

// Format a double as a fixed-precision string for table cells.
std::string fmt(double v, int precision = 4)
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(precision) << v;
    return ss.str();
}

// Mapping between UI display name and domain/biosim IDs.
// Extend as more entries are added to the registries.
struct IdMap {
    std::string displayName;
    std::string domainId;
    std::string biosimId;
};

// ponytail: small static tables — no config file needed at this scale.
const std::vector<IdMap> k_materials = {
    {"Water (liquid, ICRU)",     "water_icru",           "water_icru"},
    {"Muscle (skeletal, ICRU)",  "muscle_skeletal_icru",  "muscle_skeletal"},
    {"Bone (cortical, ICRU)",    "bone_cortical_icru",    "bone_cortical"},
    {"Air (dry)",                "air_dry",               "air_dry"},
    {"PMMA",                     "pmma",                  "pmma"},
    {"Polystyrene",              "polystyrene",           "polystyrene"},
};

const std::vector<IdMap> k_particles = {
    {"Proton",    "proton",   "proton"},
    {"Alpha",     "alpha",    "alpha"},
    {"Carbon-12", "carbon12", "carbon12"},
};

} // anonymous namespace

// ── Constructor ─────────────────────────────────────────────────────────────

ValidationView::ValidationView(QWidget* parent)
    : QWidget(parent),
      matReg_(std::make_unique<domain::materials::MaterialRegistry>()),
      partReg_(std::make_unique<domain::physics::ParticleRegistry>())
{
    matReg_->loadBuiltinMaterials();
    partReg_->loadBuiltinParticles();

    buildUi();
    populateSelectors();
}

// ── buildUi ─────────────────────────────────────────────────────────────────

void ValidationView::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(10);

    // ── Header row ──────────────────────────────────────────────────────────
    auto* header_row = new QHBoxLayout;
    header_row->setSpacing(8);

    auto* part_label = new QLabel("Partícula:", this);
    part_label->setObjectName("Muted");
    particle_combo_ = new QComboBox(this);
    particle_combo_->setFixedWidth(140);

    auto* mat_label = new QLabel("Material:", this);
    mat_label->setObjectName("Muted");
    material_combo_ = new QComboBox(this);
    material_combo_->setFixedWidth(220);

    validate_btn_ = new QPushButton("Validar", this);
    validate_btn_->setObjectName("PrimaryButton");
    validate_btn_->setFixedWidth(100);

    header_row->addWidget(part_label);
    header_row->addWidget(particle_combo_);
    header_row->addWidget(mat_label);
    header_row->addWidget(material_combo_);
    header_row->addWidget(validate_btn_);
    header_row->addStretch();
    layout->addLayout(header_row);

    // ── Table ────────────────────────────────────────────────────────────────
    table_ = new QTableWidget(this);
    table_->setAlternatingRowColors(true);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->setVisible(false);
    layout->addWidget(table_, 1);

    // ── Footer note ──────────────────────────────────────────────────────────
    footer_label_ = new QLabel(this);
    footer_label_->setObjectName("Muted");
    footer_label_->setWordWrap(true);
    footer_label_->setTextFormat(Qt::PlainText);
    layout->addWidget(footer_label_);

    // ── Connect ──────────────────────────────────────────────────────────────
    connect(validate_btn_, &QPushButton::clicked,
            this,          &ValidationView::onValidate);
}

// ── populateSelectors ───────────────────────────────────────────────────────

void ValidationView::populateSelectors()
{
    for (const auto& p : k_particles)
        particle_combo_->addItem(QString::fromStdString(p.displayName));

    for (const auto& m : k_materials)
        material_combo_->addItem(QString::fromStdString(m.displayName));
}

// ── onValidate ───────────────────────────────────────────────────────────────

void ValidationView::onValidate()
{
    const int pi = particle_combo_->currentIndex();
    const int mi = material_combo_->currentIndex();

    if (pi < 0 || pi >= static_cast<int>(k_particles.size())) return;
    if (mi < 0 || mi >= static_cast<int>(k_materials.size())) return;

    const auto& pm = k_particles[static_cast<std::size_t>(pi)];
    const auto& mm = k_materials[static_cast<std::size_t>(mi)];

    validation::NistValidator::Options opts;
    opts.domainParticleId = pm.domainId;
    opts.domainMaterialId = mm.domainId;
    opts.biosimParticleId = pm.biosimId;
    opts.biosimMaterialId = mm.biosimId;

    validation::NistValidator validator(*matReg_, *partReg_);
    const auto res = validator.validate(opts);

    // ── Build table ──────────────────────────────────────────────────────────
    table_->clearContents();

    if (res.hasNistReference) {
        // proton/water: 7 columns including NIST reference
        const QStringList headers{
            "E [MeV]",
            "NIST [MeV/cm]",
            "Dominio [MeV/cm]",
            "BioSim [MeV/cm]",
            "dev dom %",
            "dev bio %",
            "✓/✗"  // ✓/✗ — avoid raw Unicode in source for portability
        };
        table_->setColumnCount(static_cast<int>(headers.size()));
        table_->setHorizontalHeaderLabels(headers);
        table_->setRowCount(static_cast<int>(res.rows.size()));

        for (int row = 0; row < static_cast<int>(res.rows.size()); ++row) {
            const auto& r = res.rows[static_cast<std::size_t>(row)];

            auto setItem = [&](int col, const std::string& text,
                               bool colored = false, bool pass = true) {
                auto* item = new QTableWidgetItem(QString::fromStdString(text));
                item->setTextAlignment(Qt::AlignCenter);
                if (colored) {
                    // Green if pass, red if fail — dark-theme safe colours.
                    item->setForeground(
                        pass ? QColor(80, 220, 120) : QColor(230, 80, 80));
                }
                table_->setItem(row, col, item);
            };

            setItem(0, fmt(r.energy_MeV, 1));
            setItem(1, r.has_nist          ? fmt(r.nist_dEdx_MeV_cm)       : "N/D");
            setItem(2, r.domain_available  ? fmt(r.domain_dEdx_MeV_cm)     : "N/D");
            setItem(3, r.biosim_available  ? fmt(r.biosim_dEdx_MeV_cm)     : "N/D");

            if (r.has_nist && r.domain_available) {
                setItem(4, fmt(r.dev_domain_pct, 2) + " %", true, r.domain_pass);
            } else {
                setItem(4, "\xe2\x80\x94"); // — em dash
            }
            if (r.has_nist && r.biosim_available) {
                setItem(5, fmt(r.dev_biosim_pct, 2) + " %", true, r.biosim_pass);
            } else {
                setItem(5, "\xe2\x80\x94");
            }

            // Pass/fail glyph: pass only if NIST available and both engines pass.
            const bool pass = r.has_nist &&
                              (!r.domain_available || r.domain_pass) &&
                              (!r.biosim_available || r.biosim_pass);
            setItem(6, pass ? "\xe2\x9c\x93" : "\xe2\x9c\x97", true, pass); // ✓ / ✗
        }

        // Footer summary
        std::ostringstream foot;
        foot << "Validado vs NIST PSTAR (I=75 eV, ICRU-49) — ";
        if (res.maxDevDomain_pct > 0.0)
            foot << "desv. max dominio " << fmt(res.maxDevDomain_pct, 2) << " %";
        if (res.maxDevBiosim_pct > 0.0)
            foot << ", biosim " << fmt(res.maxDevBiosim_pct, 2) << " %.";
        foot << " " << res.note;
        footer_label_->setText(QString::fromStdString(foot.str()));

    } else {
        // Other combinations: 4 columns, NO NIST column
        const QStringList headers{
            "E [MeV]",
            "Dominio [MeV/cm]",
            "BioSim [MeV/cm]",
            "dif inter-motor %"
        };
        table_->setColumnCount(static_cast<int>(headers.size()));
        table_->setHorizontalHeaderLabels(headers);
        table_->setRowCount(static_cast<int>(res.rows.size()));

        for (int row = 0; row < static_cast<int>(res.rows.size()); ++row) {
            const auto& r = res.rows[static_cast<std::size_t>(row)];

            auto setItem = [&](int col, const std::string& text) {
                auto* item = new QTableWidgetItem(QString::fromStdString(text));
                item->setTextAlignment(Qt::AlignCenter);
                table_->setItem(row, col, item);
            };

            setItem(0, fmt(r.energy_MeV, 1));
            setItem(1, r.domain_available ? fmt(r.domain_dEdx_MeV_cm) : "N/D");
            setItem(2, r.biosim_available ? fmt(r.biosim_dEdx_MeV_cm) : "N/D");
            setItem(3, (r.domain_available && r.biosim_available)
                           ? fmt(r.dev_interengine_pct, 2) + " %"
                           : "N/D");
        }

        // Footer note — prominently visible
        footer_label_->setText(
            "Sin referencia NIST publicada \xe2\x80\x94 "
            "comparaci\xc3\xb3n inter-motor (consistencia). "
            + QString::fromStdString(res.note));
    }
}

} // namespace beamlab::ui
