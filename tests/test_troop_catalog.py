import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import troop_catalog as model


class TroopCatalogTests(unittest.TestCase):
    def setUp(self):
        self.catalog = model.load_catalog()

    def reject(self, change):
        value = copy.deepcopy(self.catalog)
        change(value)
        with self.assertRaises(ValueError):
            model.validate_catalog(value)

    def test_initial_independent_design(self):
        troop = self.catalog["troops"][0]
        self.assertEqual((troop["id"], troop["name"], troop["native_carrier"], troop["commanders"]),
                         ("san14.xianzhen", "陷阵营", 1, [254]))
        self.assertEqual(troop["extra_gold"], {"fixed": 2000, "per_1000_soldiers": 500})
        self.assertEqual(len(troop["effects"]), 3)
        self.assertEqual(troop["max_soldiers"],1000)

    def test_surround_descriptor_validation(self):
        self.reject(lambda c:c["troops"][0]["effects"][2].update(value_bp=9999))
        self.reject(lambda c:c["troops"][0]["effects"][2].update(status="confusion"))
        self.assertIn("S14_TROOP_SURROUND_IMMUNITY",model.native_header(self.catalog))

    def test_soldier_cap_and_legacy_catalog(self):
        for value in (0,-1,100001,True,"1000",1.5):
            self.reject(lambda c,n=value:c["troops"][0].update(max_soldiers=n))
        self.catalog["troops"][0].pop("max_soldiers")
        self.assertIn('.max_soldiers=100000U',model.native_header(model.validate_catalog(self.catalog)))

    def test_capacity_does_not_use_native_id_count(self):
        original = self.catalog["troops"][0]
        self.catalog["troops"] = [dict(copy.deepcopy(original), id=f"test.type_{i}") for i in range(256)]
        self.assertEqual(len(model.validate_catalog(self.catalog)["troops"]), 256)
        self.catalog["troops"].append(dict(copy.deepcopy(original), id="test.excess"))
        with self.assertRaises(ValueError):
            model.validate_catalog(self.catalog)

    def test_duplicate_ids(self):
        self.reject(lambda c: c["troops"].append(copy.deepcopy(c["troops"][0])))

    def test_unknown_field_is_not_silently_ignored(self):
        self.reject(lambda c: c["troops"][0].update(attack_bonus=999))

    def test_unknown_schema(self):
        self.reject(lambda c: c.update(schema_version=2))

    def test_bool_is_not_integer(self):
        self.reject(lambda c: c["troops"][0].update(native_carrier=True))

    def test_native_ids_not_extended(self):
        for value in (0, 21, 100):
            self.reject(lambda c, n=value: c["troops"][0].update(native_carrier=n))

    def test_utf8_capacity_and_controls(self):
        for value in ("汉" * 22, "a\0b", "a\nb", "a\ud800", ""):
            self.reject(lambda c, name=value: c["troops"][0].update(name=name))

    def test_whitelist_and_explicit_all(self):
        for value in ([], [254, 254], [6001], [-1], [True], list(range(65))):
            self.reject(lambda c, ids=value: c["troops"][0].update(commanders=ids))
        self.reject(lambda c: c["troops"][0].update(commander_scope="all"))
        self.catalog["troops"][0].update(commander_scope="all", commanders=[])
        model.validate_catalog(self.catalog)

    def test_fees_and_attribute_limits(self):
        for value in (-1, 1_000_000_001, "500", 0.5):
            self.reject(lambda c, n=value: c["troops"][0]["extra_gold"].update(fixed=n))
        self.reject(lambda c: c["troops"][0]["attribute_bonus_bp"].update(attack=-10000))

    def test_unrecognized_or_repeated_effect(self):
        self.reject(lambda c: c["troops"][0]["effects"][0].update(kind="heal"))
        self.reject(lambda c: c["troops"][0]["effects"].append(copy.deepcopy(c["troops"][0]["effects"][0])))
        self.reject(lambda c: c["troops"][0]["effects"][1].update(status="fire"))
        self.reject(lambda c: c["troops"][0]["effects"][1].update(value_bp=5000))

    def test_icon_is_local_existing_asset(self):
        for icon in ("../x.svg", "assets/troops/../x.svg", "assets/troops/missing.svg", "https://x/a.svg"):
            self.reject(lambda c, path=icon: c["troops"][0].update(icon=path))

    def test_duplicate_json_keys_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "troops.json"
            path.write_text('{"schema_version":1,"schema_version":1,"troops":[]}', encoding="utf-8")
            with self.assertRaises(ValueError):
                model.load_catalog(path)

    def test_invalid_catalog_does_not_replace_previous_output(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "troops.json"
            header, preview = Path(folder) / "catalog.h", Path(folder) / "preview.html"
            path.write_text(json.dumps(self.catalog), encoding="utf-8")
            model.generate(path, header, preview)
            previous = header.read_bytes(), preview.read_bytes()
            self.catalog["troops"][0]["native_carrier"] = 21
            path.write_text(json.dumps(self.catalog), encoding="utf-8")
            with self.assertRaises(ValueError):
                model.generate(path, header, preview)
            self.assertEqual(previous, (header.read_bytes(), preview.read_bytes()))

    def test_codegen_utf8_and_html_script_boundary(self):
        self.catalog["troops"][0]["description"] = "说明 </script><script>alert(1)</script>"
        model.validate_catalog(self.catalog)
        header = model.native_header(self.catalog)
        self.assertIn("S14_TROOP_CONFUSION", header)
        self.assertIn('"\\351\\231\\267', header)  # 陷 UTF-8
        preview = model.preview_html(self.catalog)
        self.assertNotIn("说明 </script>", preview)
        self.assertIn("\\u003c/script>", preview)
        self.assertIn("data:image/svg+xml;base64,", preview)
        self.assertNotIn("__S14_TROOP_CATALOG__", preview)


if __name__ == "__main__":
    unittest.main()
