import importlib.util
from pathlib import Path
import struct
import unittest


spec = importlib.util.spec_from_file_location(
    'game_interfaces', Path(__file__).resolve().parents[1] / 'tools/inventory_game_interfaces.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def sample():
    data = bytearray(0x800)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 0x3c, 0x80)
    data[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<HH', data, 0x84, 0x8664, 2)
    struct.pack_into('<H', data, 0x94, 0xf0)
    struct.pack_into('<H', data, 0x98, 0x20b)
    struct.pack_into('<Q', data, 0xb0, 0x140000000)
    struct.pack_into('<I', data, 0xd0, 0x3000)
    for at, name, rva, raw_at, size, flags in (
            (0x188, b'.text', 0x1000, 0x400, 0x100, 0x60000020),
            (0x1b0, b'.rdata', 0x2000, 0x500, 0x300, 0x40000040)):
        data[at:at + len(name)] = name
        struct.pack_into('<4I', data, at + 8, size, rva, size, raw_at)
        struct.pack_into('<I', data, at + 36, flags)
    data[0x590:0x59c] = b'.?AVCArmyAI@@'
    struct.pack_into('<6I', data, 0x540, 1, 0, 0, 0x2080, 0, 0x2040)
    # A vtable followed by a callback table cannot be separated by RTTI alone.
    struct.pack_into('<4Q', data, 0x5c0,
                     0x140002040, 0x140001000, 0x140001010, 0x140001020)
    return data


class GameInterfaceInventoryTests(unittest.TestCase):
    def test_adjacent_callbacks_are_unclassified_candidates(self):
        result = module.inventory(sample())
        self.assertEqual(result['schema_version'], 2)
        entry, = result['entries']
        self.assertEqual(entry['type'], 'CArmyAI')
        table, = entry['vtables']
        self.assertEqual(table['first_virtual_entry'], '0x1000')
        self.assertIsNone(table['method_count'])
        self.assertFalse(table['boundary_verified'])
        self.assertEqual(len(table['contiguous_code_pointer_candidates']), 3)
        self.assertNotIn('methods', table)
        self.assertIsNone(entry['signature'])
        self.assertFalse(entry['runtime_observed'])

    def test_non_code_first_entry_is_rejected(self):
        data = sample()
        struct.pack_into('<Q', data, 0x5c8, 0x140002090)
        self.assertEqual(module.inventory(data)['entries'][0]['vtables'], [])

    def test_malformed_sections_are_rejected(self):
        data = sample()
        struct.pack_into('<I', data, 0x198, 0x1000)
        with self.assertRaises(ValueError):
            module.inventory(data)
        with self.assertRaises(ValueError):
            module.inventory(b'MZ')


if __name__ == '__main__':
    unittest.main()
