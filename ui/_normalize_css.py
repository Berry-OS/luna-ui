#!/usr/bin/env python3
"""Normalize demo CSS for browser + Vespera engine parity (px units, positioning)."""
from __future__ import annotations

import re
import sys
from pathlib import Path

MARKER = "/* Vespera GUI Engine v3.10"

POSITION_HEADER = """\
/* Vespera GUI Engine v3.10 — browser-compatible shared CSS */
html, body { margin: 0; padding: 0; min-height: 100%; background: #b8b8c8; font-family: system-ui, -apple-system, sans-serif; }
body { position: relative; min-width: 980px; min-height: 820px; }
#main_win, #toast, #info_win, #dock, #modal_overlay { position: absolute; box-sizing: border-box; }
#sidebar, #content, #title_bar { position: absolute; box-sizing: border-box; }
.window { position: absolute; box-sizing: border-box; }
#tag_cloud, #shrink_row, #dense_demo, #col_dense_demo, #toolbar, #chip_row, #theme_combo,
#hscroll_panel, #activity_panel, #slider_track, #progress_track, #toast_bar, #modal_dialog,
#hscroll_inner { position: absolute; box-sizing: border-box; }
.nav_item, .btn_os, .title_drag, .badge, .dock_icon, #apply_btn, #toggle_knob_track, .toggle,
.checkbox, #theme_select_box, #select_panel, .select_option, #slider_fill, #slider_thumb,
.hscroll_chip, .activity_line, .activity_sticky, .activity_sticky_bottom,
#toast_drag, #toast_icon, #toast_close, #info_drag, #info_close, #info_icon,
#modal_icon, #modal_cancel, #modal_confirm, #sidebar_avatar, #sep1, #sep2,
#section_title, #desc, #toggle_row_label, #chk_label, #theme_label, #slider_label,
#progress_label, #status_text, #toast_title, #toast_msg, #info_title, #info_msg,
#modal_title, #modal_msg, #sidebar_username, #sidebar_role, #title_text { position: absolute; box-sizing: border-box; }
.tag, .shrink_item, .dense_cell, .col_dense_cell, .tool_btn, .chip { position: static; box-sizing: border-box; }
.toggle_knob { position: absolute; box-sizing: border-box; }
"""

SKIP_PROPS = {
    "z-index", "opacity", "font-weight", "flex-grow", "flex-shrink", "order",
    "grid-column", "grid-row", "scroll-behavior", "scroll-snap-type", "scroll-snap-align",
    "overflow", "overflow-x", "overflow-y", "display", "position", "cursor",
    "pointer-events", "visibility", "box-sizing", "text-align", "place-self",
    "grid-auto-flow", "justify-content", "align-items", "align-content", "flex-direction",
    "flex-wrap", "justify-items", "place-content", "place-items", "font-family",
    "min-height", "min-width", "max-height", "max-width",
}

GRID_TRACK_PROPS = {
    "grid-template-columns", "grid-template-rows", "grid-auto-rows", "grid-auto-columns",
}

SHORTHAND_MULTI_PX = {"scroll-padding", "scroll-margin", "margin", "padding"}


def px_token(tok: str) -> str:
    if re.fullmatch(r"-?\d+(\.\d+)?", tok):
        return tok + "px"
    return tok


def normalize_box_shadow(val: str) -> str:
    m = re.match(
        r"^(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)\s+(.+)$",
        val.strip(),
    )
    if not m:
        return val
    return f"{px_token(m.group(1))} {px_token(m.group(2))} {px_token(m.group(3))} {m.group(4)}"


def normalize_value(prop: str, val: str) -> str:
    prop = prop.strip().lower()
    val = val.strip()
    if not val or prop in SKIP_PROPS:
        return val
    if prop == "box-shadow":
        return normalize_box_shadow(val)
    if "gradient" in val or val.startswith("#") or "rgba(" in val or "rgb(" in val:
        return val
        parts = val.split()
        if len(parts) == 3 and re.fullmatch(r"\d+(\.\d+)?", parts[2]):
            parts[2] = px_token(parts[2])
        return " ".join(parts)
    if prop == "flex":
        return " ".join(px_token(t) for t in val.split())
    return " ".join(px_token(t) if re.fullmatch(r"-?\d+(\.\d+)?", t) else t for t in val.split())


def strip_header(text: str) -> str:
    text = text.replace(": hover", ":hover").replace(": active", ":active")
    text = text.replace(": focus-visible", ":focus-visible").replace(": focus-within", ":focus-within")
    anchor = text.find(".window { background-color")
    if anchor == -1:
        anchor = text.find(".window { background")
    if anchor != -1:
        return text[anchor:].lstrip()
    if MARKER in text:
        parts = text.split(MARKER)
        return parts[-1].lstrip()
    return text.lstrip()


def normalize_declarations(body: str) -> str:
    def fix_decl(match: re.Match[str]) -> str:
        prop, val = match.group(1), match.group(2)
        return f"{prop}: {normalize_value(prop, val)}"

    return re.sub(r"([\w-]+)\s*:\s*([^;]+)", fix_decl, body)


def normalize_css(text: str) -> str:
    body = strip_header(text)

    def fix_rule(match: re.Match[str]) -> str:
        selector = match.group(1)
        decls = normalize_declarations(match.group(2))
        return f"{selector}{{{decls}}}"

    body = re.sub(r"([^{]+)\{([^}]*)\}", fix_rule, body)
    return POSITION_HEADER + "\n" + body


def main() -> int:
    path = Path(sys.argv[1] if len(sys.argv) > 1 else "luna-ui.css")
    raw = path.read_text(encoding="utf-8")
    path.write_text(normalize_css(raw), encoding="utf-8")
    print(f"Normalized: {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
