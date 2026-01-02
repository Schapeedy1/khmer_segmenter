import sys
import os

# Add parent directory to path
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
from khmer_segmenter import KhmerSegmenter

def test_separators():
    # Setup
    data_dir = os.path.join(os.path.dirname(__file__), '..', 'data')
    dict_path = os.path.join(data_dir, "khmer_dictionary_words.txt")
    freq_path = os.path.join(data_dir, "khmer_word_frequencies.json")
    
    if not os.path.exists(dict_path):
        print(f"Dictionary not found at {dict_path}")
        return

    segmenter = KhmerSegmenter(dict_path, freq_path)
    
    # User case: "ដើម˝"
    # Char ˝ seems to be U+02DD ?
    text = "ដើម˝"
    quote_char = text[-1]
    print(f"Quote Char: '{quote_char}' (Ord: {hex(ord(quote_char))})")
    
    result = segmenter.segment(text)
    print(f"Input: {text}")
    print(f"Result: {result}")
    
    if len(result) == 2 and quote_char in result:
        print("PASSED: Quote marked as separate segment.")
    else:
        print("FAILED: Quote merged or missed.")

    # Check "ដើម''" (Standard quotes)
    text2 = "ដើម''"
    result2 = segmenter.segment(text2)
    print(f"Input: {text2}")
    print(f"Result: {result2}")

if __name__ == "__main__":
    test_separators()
