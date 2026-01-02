import unittest
import sys
import os

# Add parent directory to path to import khmer_segmenter package
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
from khmer_segmenter import KhmerSegmenter

class TestHeuristics(unittest.TestCase):
    def setUp(self):
        script_dir = os.path.dirname(os.path.abspath(__file__))
        project_root = os.path.dirname(script_dir)
        data_dir = os.path.join(project_root, 'data')
        dict_path = os.path.join(data_dir, "khmer_dictionary_words.txt")
        freq_path = os.path.join(data_dir, "khmer_word_frequencies.json")
        self.segmenter = KhmerSegmenter(dict_path, freq_path)

    def test_quote_separation(self):
        # Rule 3
        text = '“សួស្តី”'
        segments = self.segmenter.segment(text)
        print(f"Quote Test: {text} -> {segments}")
        self.assertIn('“', segments)
        self.assertIn('”', segments)
        self.assertIn('សួស្តី', segments)
        # Check order
        self.assertEqual(segments, ['“', 'សួស្តី', '”'])

    def test_repetition_mark(self):
        # Rule 4
        text = 'ផ្សេងៗ'
        segments = self.segmenter.segment(text)
        print(f"Repetition Test: {text} -> {segments}")
        self.assertIn('ៗ', segments)
        self.assertIn('ផ្សេង', segments)
        # Should be ['ផ្សេង', 'ៗ']
        self.assertEqual(segments, ['ផ្សេង', 'ៗ'])

    def test_merge_suffix(self):
        # Rule 1: Consonant + [់, ិ៍, ៍, ៌] combined with previous
        # Assuming 'ធម' is a chunk and '៍' (or 'ម៍') matches pattern?
        # Actually logic is: Current segment is len 2 (Cons+Sign), merge with Prev.
        # Construct input where 'ធម' is separated from 'ម៌' or similar?
        # Let's try 'ធម៌' directly. If it's in dict, it's fine.
        # We need a case where it breaks.
        # 'សង្ឃ' + 'ម' + '៍'? -> 'សង្ឃ' + 'ម៍' -> 'សង្ឃម៍'?
        # Let's try 'ឧប' + 'ធម' + '៍' (if ធម៍ is not in dict)
        # NOTE: ធម៌ is in dict.
        pass

    def test_merge_prefix(self):
        # Rule 2: Consonant + ័ combined with NEXT
        # e.g. 'សម័' + 'យ' -> 'សម័យ'
        # Construct input: 'សម័យ' (if not in dict or if we force split)
        pass

if __name__ == '__main__':
    unittest.main()
