#!/usr/bin/env bash
# ══════════════════════════════════════════════════════════════════════
#  create_dummy_plugin_so.sh
#  Compiles a minimal IPlugin shared library for PluginHost tests.
#  Used by test_PluginHost::LoadFromSharedLibrary.
#
#  Usage:
#    bash tests/unit/platform/create_dummy_plugin_so.sh <output_dir>
#
#  Example:
#    bash tests/unit/platform/create_dummy_plugin_so.sh /tmp/beamlab_test_plugins
#    → /tmp/beamlab_test_plugins/libdummy.so
# ══════════════════════════════════════════════════════════════════════

set -euo pipefail

OUT_DIR="${1:-/tmp/beamlab_test_plugins}"
mkdir -p "$OUT_DIR"

SOURCE=$(cat << 'CPPEOF'
#include "platform/IPlugin.h"
#include "app/ApplicationContext.h"

using namespace beamlab::platform;

class DummyPlugin final : public IPlugin {
public:
    std::string name() const override { return "Dummy"; }
    std::string version() const override { return "1.0.0"; }
    std::string description() const override { return "External test plugin"; }
    void initialize(ApplicationContext&) override {}
    void shutdown() override {}
};

extern "C" __attribute__((visibility("default"))) IPlugin* create_plugin() {
    return new DummyPlugin();
}
CPPEOF
)

SRC_FILE="$OUT_DIR/dummy_plugin.cpp"
echo "$SOURCE" > "$SRC_FILE"

BEAMLAB_SRC_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
LIB_FILE="$OUT_DIR/libdummy.so"

c++ -std=c++17 -fPIC -shared \
    -I"$BEAMLAB_SRC_DIR/src" \
    -o "$LIB_FILE" "$SRC_FILE" \
    -Wno-unused-parameter

echo "✅ Created: $LIB_FILE"
nm -D "$LIB_FILE" | grep create_plugin && echo "✅ Symbol 'create_plugin' exported"
