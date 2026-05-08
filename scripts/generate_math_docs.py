#!/usr/bin/env python3
"""
BeamLabStudio — Generador de documentación matemática.

Lee todos los archivos .cpp y .h bajo src/ buscando bloques //!@math-begin ... //!@math-end
y genera:
  - assets/math_docs.html   (HTML embebido en QRC)

Formato de los bloques en el código C++:
  //!@math-begin module="ID" title="Título Humano"
  //!@math  Texto de descripción libre.
  //!@formula  Expresión matemática (se muestra en recuadro)
  //!@var  variable = descripción
  //!@note  Nota aclaratoria
  //!@section Subtítulo de sección
  //!@math-end
"""

import re
import sys
from pathlib import Path
from datetime import datetime
from typing import NamedTuple

# ── Configuración ─────────────────────────────────────────────────────────────
REPO_ROOT   = Path(__file__).parent.parent
SRC_DIR     = REPO_ROOT / "src"
OUTPUT_HTML = REPO_ROOT / "src" / "ui" / "qt" / "assets" / "math_docs.html"

# ── Modelos ───────────────────────────────────────────────────────────────────
class Block(NamedTuple):
    module: str
    title:  str
    source: str   # relative path of the source file
    lines:  list  # list of (tag, content) tuples


def parse_blocks(src_dir: Path) -> list:
    blocks = []
    pattern_begin = re.compile(
        r'//!@math-begin\s+module="([^"]+)"\s+title="([^"]+)"'
    )
    tag_re = re.compile(r'//!@(\S+)\s*(.*)')

    for path in sorted(src_dir.rglob("*.cpp")) + sorted(src_dir.rglob("*.h")):
        text = path.read_text(encoding="utf-8", errors="replace")
        lines = text.splitlines()

        i = 0
        while i < len(lines):
            m = pattern_begin.search(lines[i])
            if m:
                module, title = m.group(1), m.group(2)
                block_lines = []
                i += 1
                while i < len(lines):
                    end_m = re.search(r'//!@math-end', lines[i])
                    if end_m:
                        i += 1
                        break
                    tag_m = tag_re.search(lines[i])
                    if tag_m:
                        block_lines.append((tag_m.group(1), tag_m.group(2).strip()))
                    i += 1
                rel = path.relative_to(REPO_ROOT)
                blocks.append(Block(module, title, str(rel), block_lines))
            else:
                i += 1
    return blocks


# ── Generación HTML ───────────────────────────────────────────────────────────
CSS = """
body {
    font-family: 'Segoe UI', Arial, sans-serif;
    background: #1A1E2A;
    color: #C8D4E8;
    margin: 0;
    padding: 20px 28px;
    font-size: 14px;
    line-height: 1.65;
}
h1 { color: #7CF2D2; font-size: 1.4em; margin-bottom: 4px; }
h2 { color: #7CAFF2; font-size: 1.15em; margin-top: 28px;
     border-bottom: 1px solid #2E3650; padding-bottom: 4px; }
h3 { color: #A0C0FF; font-size: 1.0em; margin-top: 18px; margin-bottom: 4px; }
p  { margin: 4px 0 8px 0; }
.formula {
    display: block;
    background: #12172A;
    border-left: 3px solid #7CF2D2;
    padding: 8px 14px;
    margin: 8px 0 8px 0;
    font-family: 'Consolas', 'Courier New', monospace;
    font-size: 0.97em;
    color: #E8F4FF;
    white-space: pre-wrap;
}
.var {
    display: block;
    padding: 2px 0 2px 18px;
    color: #B0C8E8;
    font-family: 'Consolas', 'Courier New', monospace;
    font-size: 0.93em;
}
.note {
    display: block;
    padding: 3px 0 3px 14px;
    border-left: 2px solid #445;
    color: #90A8C4;
    font-style: italic;
    margin: 4px 0;
}
.source {
    font-size: 0.78em;
    color: #4A6080;
    margin-bottom: 12px;
    font-family: 'Consolas', monospace;
}
.toc { margin: 16px 0 24px 0; padding: 12px 18px;
       background: #141822; border-radius: 6px; }
.toc a { color: #7CAFF2; text-decoration: none; display: block;
          padding: 2px 0; }
.toc a:hover { color: #7CF2D2; }
.timestamp { color: #3A5070; font-size: 0.8em; margin-top: 40px; }
hr { border: none; border-top: 1px solid #232A3A; margin: 24px 0; }
"""

def esc(s: str) -> str:
    """Minimal HTML escaping (already escaped in source via &lt; etc.)."""
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def render_block(block: Block) -> str:
    html = [f'<h2 id="mod-{block.module}">{esc(block.title)}</h2>']
    html.append(f'<div class="source">⟵ {esc(block.source)}</div>')

    for tag, content in block.lines:
        if tag == "math":
            html.append(f"<p>{esc(content)}</p>" if content else "")
        elif tag == "formula":
            html.append(f'<code class="formula">{esc(content)}</code>')
        elif tag == "var":
            html.append(f'<code class="var">  {esc(content)}</code>')
        elif tag == "note":
            html.append(f'<span class="note">ℹ {esc(content)}</span>')
        elif tag == "section":
            html.append(f"<h3>{esc(content)}</h3>")

    return "\n".join(html)


def build_html(blocks: list) -> str:
    toc = ['<div class="toc"><strong>Contenido</strong><br/>']
    for b in blocks:
        toc.append(f'<a href="#mod-{b.module}">{esc(b.title)}</a>')
    toc.append("</div>")

    body_parts = []
    for i, b in enumerate(blocks):
        if i > 0:
            body_parts.append("<hr/>")
        body_parts.append(render_block(b))

    ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    body_parts.append(
        f'<p class="timestamp">Generado automáticamente el {ts} '
        f'a partir de los bloques //!@math en el código fuente.</p>'
    )

    content = "\n".join(toc) + "\n" + "\n".join(body_parts)

    return f"""<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8"/>
<title>BeamLabStudio — Documentación Matemática</title>
<style>{CSS}</style>
</head>
<body>
<h1>BeamLabStudio — Procedimientos Matemáticos</h1>
<p>Esta documentación se extrae automáticamente de los bloques
<code>//!@math-begin</code> en el código fuente.
Al recompilar el proyecto los cálculos mostrados aquí reflejan exactamente
las fórmulas implementadas.</p>
{content}
</body>
</html>
"""


# ── Main ──────────────────────────────────────────────────────────────────────
def main():
    blocks = parse_blocks(SRC_DIR)

    if not blocks:
        print("WARN: No se encontraron bloques //!@math-begin en src/", file=sys.stderr)

    OUTPUT_HTML.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_HTML.write_text(build_html(blocks), encoding="utf-8")

    print(f"[math_docs] {len(blocks)} módulos → {OUTPUT_HTML}")


if __name__ == "__main__":
    main()
