"""Validate troop definitions and generate the native catalog and local preview.

Definitions are build-time configuration. This tool never opens the game,
patches native formation tables, installs anything, or spends currency.
"""
from __future__ import annotations

import argparse
import base64
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
# Matches the native army attribute package, not the visual column order.
ATTRIBUTES = ("attack", "siege_attack", "siege_break", "mobility", "defense")
NATIVE_NAMES = (
    "无效", "大戟", "重骑", "弓弩", "刀盾", "长枪", "弓骑", "突骑", "井阑", "冲车", "投石",
    "走舸", "艨艟", "楼船", "四夷", "亲卫", "流民", "货船", "货船", "辎重", "货船",
)
TROOP_KEYS = {
    "id", "revision", "name", "description", "native_carrier", "commander_scope",
    "commanders", "unlock_key", "icon", "attribute_bonus_bp", "effects", "extra_gold",
}


def fail(message: str) -> None:
    raise ValueError(message)


def keys(value: object, expected: set[str], where: str) -> dict:
    if not isinstance(value, dict) or set(value) != expected:
        fail(f"{where}: expected exactly {sorted(expected)}")
    return value


def integer(value: object, low: int, high: int, where: str) -> int:
    if type(value) is not int or not low <= value <= high:
        fail(f"{where}: expected integer {low}..{high}")
    return value


def text(value: object, capacity: int, where: str, identifier: bool = False) -> str:
    if not isinstance(value, str) or not value:
        fail(f"{where}: expected nonempty string")
    try:
        encoded = value.encode("utf-8", "strict")
    except UnicodeError:
        fail(f"{where}: invalid Unicode")
    if len(encoded) >= capacity or any(ord(c) < 32 or ord(c) == 127 for c in value):
        fail(f"{where}: UTF-8 text exceeds {capacity - 1} bytes or contains controls")
    if identifier and not re.fullmatch(r"[a-z][a-z0-9._-]*", value):
        fail(f"{where}: expected stable lowercase ID")
    return value


def validate_catalog(value: object, root: Path = ROOT) -> dict:
    catalog = keys(value, {"schema_version", "troops"}, "catalog")
    integer(catalog["schema_version"], 1, 1, "schema_version")
    troops = catalog["troops"]
    if not isinstance(troops, list) or not 1 <= len(troops) <= 256:
        fail("troops: expected 1..256 definitions")
    seen = set()
    for index, troop in enumerate(troops):
        where = f"troops[{index}]"
        keys(troop, TROOP_KEYS | ({"max_soldiers"} if isinstance(troop,dict) and "max_soldiers" in troop else set()), where)
        troop_id = text(troop["id"], 64, f"{where}.id", True)
        if troop_id in seen:
            fail(f"{where}.id: duplicate {troop_id}")
        seen.add(troop_id)
        integer(troop["revision"], 1, 2**32 - 1, f"{where}.revision")
        text(troop["name"], 64, f"{where}.name")
        text(troop["description"], 384, f"{where}.description")
        text(troop["unlock_key"], 64, f"{where}.unlock_key", True)
        integer(troop["native_carrier"], 1, 20, f"{where}.native_carrier")
        if "max_soldiers" in troop:
            integer(troop["max_soldiers"], 1, 100000, f"{where}.max_soldiers")
        scope, commanders = troop["commander_scope"], troop["commanders"]
        if scope not in ("all", "whitelist") or not isinstance(commanders, list):
            fail(f"{where}: invalid commander scope")
        if (scope == "all" and commanders) or (scope == "whitelist" and not 1 <= len(commanders) <= 64):
            fail(f"{where}: commander list does not match scope")
        for commander in commanders:
            integer(commander, 0, 6000, f"{where}.commanders")
        if len(set(commanders)) != len(commanders):
            fail(f"{where}: duplicate commander")
        icon = text(troop["icon"], 128, f"{where}.icon")
        if not re.fullmatch(r"assets/troops/[a-z0-9_.-]+\.svg", icon) or ".." in icon:
            fail(f"{where}.icon: expected local assets/troops/*.svg")
        path = (root / icon).resolve()
        if not path.is_relative_to((root / "assets/troops").resolve()) or not path.is_file():
            fail(f"{where}.icon: missing local SVG")
        bonuses = keys(troop["attribute_bonus_bp"], set(ATTRIBUTES), f"{where}.attribute_bonus_bp")
        for attribute in ATTRIBUTES:
            integer(bonuses[attribute], -9000, 30000, f"{where}.{attribute}")
        fees = keys(troop["extra_gold"], {"fixed", "per_1000_soldiers"}, f"{where}.extra_gold")
        for fee in fees:
            integer(fees[fee], 0, 1_000_000_000, f"{where}.extra_gold.{fee}")
        effects = troop["effects"]
        if not isinstance(effects, list) or len(effects) > 8:
            fail(f"{where}.effects: expected at most 8 descriptors")
        seen_effects = set()
        for effect in effects:
            if not isinstance(effect, dict):
                fail(f"{where}.effects: expected object")
            if effect.get("kind") == "damage_reduction":
                keys(effect, {"kind", "value_bp"}, f"{where}.effect")
                integer(effect["value_bp"], 1, 9000, f"{where}.damage_reduction")
            elif effect.get("kind") == "surround_immunity":
                keys(effect, {"kind", "value_bp"}, f"{where}.effect")
                integer(effect["value_bp"], 10000, 10000, f"{where}.surround_immunity")
            elif effect.get("kind") == "status_immunity":
                keys(effect, {"kind", "status", "value_bp"}, f"{where}.effect")
                if effect["status"] != "confusion":
                    fail(f"{where}.effect: unrecognized status")
                integer(effect["value_bp"], 10000, 10000, f"{where}.status_immunity")
            else:
                fail(f"{where}.effect: unrecognized effect")
            key = (effect["kind"], effect.get("status"))
            if key in seen_effects:
                fail(f"{where}: duplicate effect {key}")
            seen_effects.add(key)
    return catalog


def unique_object(pairs: list[tuple[str, object]]) -> dict:
    value = {}
    for key, item in pairs:
        if key in value:
            fail(f"duplicate JSON key: {key}")
        value[key] = item
    return value


def load_catalog(path: Path = ROOT / "data/troops.json", root: Path = ROOT) -> dict:
    if path.stat().st_size > 1_048_576:
        fail("catalog exceeds 1 MiB")
    value = json.loads(path.read_text(encoding="utf-8-sig"), object_pairs_hook=unique_object,
                       parse_constant=lambda value: fail(f"invalid JSON constant: {value}"))
    return validate_catalog(value, root)


def c_string(value: str) -> str:
    # Fixed-width octal escapes avoid encoding assumptions and adjacent hex digits.
    return '"' + "".join(f"\\{byte:03o}" for byte in value.encode("utf-8")) + '"'


def native_header(catalog: dict) -> str:
    lines = ["/* Generated by tools/troop_catalog.py from data/troops.json. */",
             "static const S14TroopDefinition s14_builtin_troops[]={"]
    for troop in catalog["troops"]:
        lines.append("    {")
        for key in ("id", "name", "description", "unlock_key", "icon"):
            lines.append(f"        .{key}={c_string(troop[key])},")
        scope = "S14_TROOP_ANY_COMMANDER" if troop["commander_scope"] == "all" else "S14_TROOP_COMMANDER_WHITELIST"
        commanders = ",".join(str(v) for v in troop["commanders"]) or "0"
        bonuses = ",".join(str(troop["attribute_bonus_bp"][key]) for key in ATTRIBUTES)
        lines += [f"        .revision={troop['revision']}U,.native_carrier={troop['native_carrier']},",
                  f"        .max_soldiers={troop.get('max_soldiers',100000)}U,",
                  f"        .commander_scope={scope},.commander_count={len(troop['commanders'])},",
                  f"        .commanders={{{commanders}}},.bonus_bp={{{bonuses}}},",
                  f"        .effect_count={len(troop['effects'])},.effects={{"]
        for effect in troop["effects"]:
            kind = {"damage_reduction": "S14_TROOP_DAMAGE_REDUCTION", "status_immunity": "S14_TROOP_STATUS_IMMUNITY", "surround_immunity": "S14_TROOP_SURROUND_IMMUNITY"}[effect["kind"]]
            status = "S14_TROOP_CONFUSION" if "status" in effect else "0"
            lines.append(f"            {{{kind},{status},{effect['value_bp']}}},")
        if not troop["effects"]:
            lines.append("            {0,0,0},")
        lines += ["        },", f"        .fixed_gold={troop['extra_gold']['fixed']}ULL,"
                  f".gold_per_1000={troop['extra_gold']['per_1000_soldiers']}ULL", "    },"]
    lines.append("};")
    return "\n".join(lines) + "\n"


def preview_html(catalog: dict, root: Path = ROOT) -> str:
    # JSON is not HTML; prevent a text field from closing the script element.
    enriched = json.loads(json.dumps(catalog, ensure_ascii=False))
    for troop in enriched["troops"]:
        troop["carrier_name"] = NATIVE_NAMES[troop["native_carrier"]]
        troop["icon_data"] = "data:image/svg+xml;base64," + base64.b64encode((root / troop["icon"]).read_bytes()).decode("ascii")
    encoded = json.dumps(enriched, ensure_ascii=False).replace("<", "\\u003c").replace("\u2028", "\\u2028").replace("\u2029", "\\u2029")
    return (ROOT / "tools/troop_preview.html").read_text(encoding="utf-8").replace("__S14_TROOP_CATALOG__", encoded)


def generate(catalog: Path, header: Path, preview: Path) -> dict:
    # Validate the WHOLE document before replacing either output.
    value = load_catalog(catalog)
    header_text, html = native_header(value), preview_html(value)
    for target, content in ((header, header_text), (preview, html)):
        target.parent.mkdir(parents=True, exist_ok=True)
        temp = target.with_suffix(target.suffix + ".tmp")
        temp.write_text(content, encoding="utf-8")
        temp.replace(target)
    return value


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", type=Path, default=ROOT / "data/troops.json")
    parser.add_argument("--header", type=Path, default=ROOT / "native/build/troop_catalog.h")
    parser.add_argument("--preview", type=Path, default=ROOT / "native/build/troops-preview.html")
    args = parser.parse_args()
    try:
        value = generate(args.catalog, args.header, args.preview)
    except (ValueError, OSError) as error:
        parser.exit(1, f"Troop catalog rejected: {error}\n")
    print(json.dumps({"status": "generated", "troops": len(value["troops"]),
                      "schema_version": 1, "header": str(args.header), "preview": str(args.preview),
                      "gameplay_integrated": False}, ensure_ascii=False))


if __name__ == "__main__":
    main()
