# Khmer Word Segmentation Algorithm

This project implements a probabilistic word segmentation algorithm for the Khmer language. It uses a **Viterbi** approach (finding the shortest path in a graph of possible segments) weighted by word probabilities derived from a text corpus.


> [!IMPORTANT]
> **Disclaimer:** This dictionary is still lacking many curated sources of technical words. If anyone can contribute curated Khmer words with credible sources, the algorithm will improve significantly. We highly appreciate your contributions to improving the data quality!

## Purpose & Design Philosophy

The primary goal of this project is **dictionary-accurate segmentation**. Unlike modern Machine Learning (ML) models that prioritize predicting conversational "intent" or deep semantic context, `KhmerSegmenter` focuses on strictly aligning text with curated, approved Khmer wording sources.

### Why Viterbi over Deep Learning?

In the current NLP landscape (2026), there is a significant trade-off between **Contextual Awareness** (Deep Learning) and **Deterministic Efficiency** (Algorithmic).

| Feature | KhmerSegmenter (Viterbi) | ML-Based (Transformers/BERT) |
| :--- | :--- | :--- |
| **Logic** | "Search": Find the mathematically best path through a curated dictionary. | "Patterns": Infer boundaries based on patterns seen in millions of articles. |
| **Transparency** | **White Box**: If a word splits incorrectly, you simply update the dictionary or frequency table. | **Black Box**: Errors require retraining with thousands of examples; shifts are often opaque. |
| **Hardware** | **Ultra-Light**: Runs on anything (Drones, Mobile, Arduinos, Low-power CPUs). | **Heavy**: Usually requires GPUs or high-end CPUs and massive RAM. |
| **Size** | **Tiny**: ~1MB (Dictionary size) + a few KB of logic. | **Massive**: 500MB to 10GB+ of model weights. |
| **Determinism** | **100% Consistent**: Same input + Same dict always equals Same output. | **Stochastic**: Can "hallucinate" or vary results based on subtle context shifts. |

### The "Context" Argument
Critics of Viterbi often point out its "Blindness" to semantic context (long-range dependencies). However, for technical documentation, standard literature, and dictionary-driven applications, this "blindness" is a **feature**:
*   It ensures that the segmenter never "imagines" words or slang not approved in your curated source.
*   It provides a high-performance baseline (95% accuracy for standard text) for a fraction of the computational cost.

### Bridging the Engineering Gap (Beyond Computer Science)
In many engineering fields—such as **Robotics, UAV/Drone systems, and Industrial Embedded Control**—there is effectively **zero support** for the Khmer language. While Computer Science has moved toward massive Machine Learning models, these "modern" solutions are impossible to run on the low-level microcontrollers and embedded processors that power real-world machinery.

This creates a digital divide: Khmer becomes a "computer-only" language, excluded from the hardware that engineers use every day. 

`KhmerSegmenter` aims to break this barrier. By using the Viterbi algorithm—a purely mathematical and algorithmic approach—we provide a solution that can be implemented in **C, C++, or Rust** and run on devices with only a few megabytes (or even kilobytes) of memory. This project isn't just about NLP; it's about making Khmer a viable language for the next generation of physical engineering.

Ultimately, `KhmerSegmenter` is designed for **portability and control**. It is the "Swiss Army Knife" of Khmer NLP—small, sharp, and reliable.

## Installation

To install the required dependencies, run:

```bash
pip install -r requirements.txt
```

## C Port (High Performance)

For users requiring maximum performance or embedding in C/C++/Zig applications, a native port is available in the `c_port/` directory.

*   **Speed**: ~1.5x faster (Single Thread), ~7x faster (Multi-Thread).
*   **Documentation**: See [c_port/README.md](c_port/README.md).
## 1. Data Preparation (`scripts/generate_frequencies.py`)

Before the segmenter works, it needs a statistical model of the language:

1.  **Input Corpora**: The system reads raw Khmer text from files like `data/khmer_wiki_corpus.txt`, `data/khmer_folktales_extracted.txt`, and `data/allwords.txt`.
2.  **Dictionary Filtering**: It loads `data/khmer_dictionary_words.txt` and filters out single-character words that are not true Khmer **Base Characters** (Consonants or Independent Vowels). This prevents signs or fragments from being treated as words.
3.  **Frequency Generation**:
    *   The `scripts/generate_frequencies.py` script supports two modes:
        *   `--engine khmernltk` (Default): Uses the external library `khmernltk` to create a baseline frequency map from scratch.
        *   `--engine internal`: Uses `KhmerSegmenter` itself. This is for **self-improvement**: once you have a baseline frequency file, you can run this to re-segment the corpus *using* that baseline to find potentially better or more consistent word counts, creating a feedback loop.
    *   **Usage**:
        ```bash
        # Bootstrap baseline
        python scripts/generate_frequencies.py --engine khmernltk --corpus data/khmer_wiki_corpus.txt --dict data/khmer_dictionary_words.txt --output data/khmer_word_frequencies.json
        
        # Improve using internal segmenter (after baseline exists)
        python scripts/generate_frequencies.py --engine internal --corpus data/khmer_wiki_corpus.txt --dict data/khmer_dictionary_words.txt --output data/khmer_word_frequencies.json
        ```
    *   It re-scans the tokens and attempts to combine them to match the **longest possible entry** in our dictionary. This helps correct potential errors in the initial tokenization and ensures our frequencies align with our specific dictionary.
    *   We calculate the count of each word and export this to `data/khmer_word_frequencies.json`.

### Updating the Dictionary

To add or modify words in the dictionary:

1.  **Edit the Dictionary File**: Open `data/khmer_dictionary_words.txt` and add/edit the words. Ensure there is only one word per line.
2.  **Regenerate Frequencies**: Run the frequency generation script to update the statistical model. This ensures the segmenter knows about the new word and its usage probability.
    ```bash
    python scripts/generate_frequencies.py --engine internal --corpus data/khmer_wiki_corpus.txt --dict data/khmer_dictionary_words.txt --output data/khmer_word_frequencies.json
    ```

## 2. The Segmentation Algorithm (`khmer_segmenter/viterbi.py`)

The core engine is a class `KhmerSegmenter` that uses **Viterbi Algorithm** (Dynamic Programming) to find the sequence of words with the *lowest total cost*.

### Cost Calculation
*   **Cost** = $-\log_{10}(Probability)$
*   **Probability**: The frequency of the word in our generated `khmer_word_frequencies.json` divided by total tokens.
*   **Unknown Word Cost**: A fixed high penalty (higher than any known word) to discourage splitting into unknown chunks if a known word is available.

---

### Step-by-Step Logic
When `segment(text)` is called:

#### Phase 0: Input Normalization (`khmer_segmenter/normalization.py`)
Before segmentation begins, the text passes through a **Khmer-specific Normalization Layer**. This addresses the "Visual vs. Logical" ordering conflict common in Khmer digital text (e.g., typing a vowel before a subscript or mixing up sign order).

1.  **Composite Vowel Merging**: 
    Automatically merges split vowel components into single canonical Unicode points:
    *   `េ` (`\u17C1`) + `ី` (`\u17B8`) $\rightarrow$ **ើ** (`\u17BE`)
    *   `េ` (`\u17C1`) + `ា` (`\u17B6`) $\rightarrow$ **ោ** (`\u17C4`)

2.  **Cluster-Based Canonical Reordering**:
    The normalizer parses text into linguistic clusters and strictly enforces the following Unicode order:
    1.  **Base Char** (Consonant or Independent Vowel)
    2.  **Subscripts (Non-Ro)** (e.g., `្ក`, `្ខ`)
    3.  **Subscript Ro** (Coeng Ro `\u17D2\u179A` is *always* moved after other subscripts)
    4.  **Registers** (Muusikatoan `៉`, Triisap `៊`) — *Crucially moved to precedes vowels to prevent display bugs*
    5.  **Dependent Vowels** (e.g., `ា`, `ិ`, `ុ`)
    6.  **Signs/Diacritics** (e.g., `ំ`, `ះ`, `៍`)

3.  **Stability & Error Handling**: 
    *   **ZWS Cleaning**: Removes Zero Width Spaces (`\u200b`) at the very first step to prevent segmentation interference.
    *   **Stable Sorting**: If multiple vowels or signs are present, they are sorted by Unicode code point to ensure deterministic output.
    *   **Orphan Preservation**: Vowels or signs without a base consonant (typos) are preserved rather than deleted, allowing the segmenter to flag them as unknown.

#### Phase 1: Viterbi Forward Pass
The algorithm iterates through the text, finding the most probabilistic path where

**Cost** = $-\log_{10}(Probability)$.

**1. Number & Currency Grouping (Highest Priority)**:
*   **Logic**: Captures digits (Khmer/Arabic), separators (`,`, `.`), and units (e.g., `1 000 000`, `1,200.50`).
*   **Currency**: Automatically groups **leading currency symbols** (e.g., `$50.00`) as a single token.
*   **Cost**: Low penalty to ensure numbers stay together.

**2. Separator Handling**:
*   **Logic**: Recognizes punctuation (`។`, `៕`, `៖`, `?`, `/`), symbols (`%`), and whitespace.
*   **Exception**: Currency symbols are only treated as separators if they *don't* precede a number.

**3. Acronym Detection**:
*   **Logic**: Identifies sequences of (Cluster + `.`) like `ស.ភ.ភ.ព.` or `គ.ម.` as single logical units.

**4. Dictionary Match (Shortest Path)**:
*   **Logic**: Matches words from the dictionary, including **automatically generated variants** for:
    *   **Interchangeable Consonants**: `្ + ត` vs `្ + ដ` (Ta/Da).
    - **Subscript Ordering**: Flexible ordering for `Coeng Ro` (e.g., `្រ` vs other subscripts).
*   **Cost**: Derived from corpus frequency.

**5. Unknown Cluster Fallback**:
*   **Logic**: If no dictionary word matches, it falls back to a structural Khmer cluster.
*   **Constraint**: Single Khmer characters (e.g., `ក`) incur an **extra penalty** unless they are valid **Base Characters** (Consonants `U+1780-U+17A2` or Independent Vowels `U+17A3-U+17B3`). This encourages merging with neighbors to avoid over-segmentation of signs or dependent vowels.

**6. Robust Recovery (Repair Mode)**:
*   **Problem**: Typos like "orphan" subscripts or vowels at the start of a boundary.
*   **Action**: Strictly consumes 1 character with a high penalty to ensure the algorithm never crashes on malformed text.

---

#### Phase 2: Backtracking
Traces the optimal path from the end of the text back to the beginning to produce the raw segments.

---

#### Phase 3: Post-Processing Rules
The raw output is refined using linguistic heuristics:

**A. Snap Isolated Consonants**:
*   Loose consonants (unknown names/typos) are attached to the previous word.
*   **Exception**: If a consonant is explicitly surrounded by spaces or separators (e.g., `... , ក , ...`), it is preserved as an isolated unit.

**B. Heuristic Sign Merging**:
*   Merges clusters with special ending signs (like `់`, `៍`, `៌`, `័`) that were not caught by the dictionary (common in names and loans).

**C. Merge Consecutive Unknowns**:
*   Groups multiple unknown clusters into a single logical "Unknown Block," typically representing a novel proper name or technical term.

---

## 3. Rule-Based Engine (Post-Processing)

The **Rule-Based Engine** (`khmer_segmenter/rule_engine.py`) is a logic layer that runs after the Viterbi algorithm. It allows you to define deterministic fixes for common errors using a JSON configuration.

### How it Works
1.  **Iterative Processing**: The engine scans the list of segments produced by Phase 1 & 2.
2.  **Priority-Based**: Rules in `khmer_segmenter/rules.json` are sorted and applied by priority (highest first).
3.  **Trigger & Action**: If a segment matches a rule's `trigger`, and satisfies its `checks` (contextual conditions), the specified `action` is performed.

### Rule Schema (`rules.json`)
| Field | Description |
| :--- | :--- |
| **`name`** | Unique identifier for the rule. |
| **`priority`** | Higher numbers run first. |
| **`trigger`** | Condition to activate the rule: `exact_match`, `regex`, or `complexity_check`. |
| **`checks`** | Contextual conditions targeting `prev` or `next` segments. |
| **`action`** | Operation to perform: `merge_prev`, `merge_next`, or `keep`. |

### Regex Example Rules
Using `regex` triggers allows targeting broad linguistic patterns:

#### A. Merge Orphaned Signs
Merges a cluster with a terminal sign (like Robat `៌`) to the previous word if the segmenter accidentally split them.
```json
{
    "name": "Merge Orphaned Signs",
    "priority": 90,
    "trigger": { "type": "regex", "value": "^[\\u1780-\\u17A2][\\u17CB\\u17CC\\u17CD\\u17CE\\u17CF]$" },
    "checks": [{ "target": "prev", "exists": true }],
    "action": "merge_prev"
}
```

#### B. Technical ID Merger
Keep alphanumeric IDs (like `#ID123`) together by merging them with the following segment.
```json
{
    "name": "Technical ID Merger",
    "priority": 70,
    "trigger": { "type": "regex", "value": "^[A-Za-z0-9#@._-]+$" },
    "checks": [{ "target": "next", "exists": true }],
    "action": "merge_next"
}
```

---

## 4. Concrete Examples

### Example 1: Known Words
**Input**: `កងកម្លាំងរក្សាសន្តិសុខ` (Security Forces)
1.  **Viterbi**: Finds `កងកម្លាំង` (Known Compound), `រក្សា` (Known), `សន្តិសុខ` (Known).
2.  **Path**: The path `កងកម្លាំង` -> `រក្សា` -> `សន្តិសុខ` has the lowest cost.
3.  **Result**: `កងកម្លាំង` | `រក្សា` | `សន្តិសុខ`

### Example 2: Names & Foreign Words
**Input**: `លោក ចន ស្មីត` (Mr. John Smith)
1.  **Viterbi**:
    *   `លោក`: Known.
    *   `ចន`: Known (John).
    *   `ស្មី`: Known (Ray/Light).
    *   `ត`: Known (Connector/Per).
    *   *Note*: Since `ស្មី` and `ត` are valid words, the segmenter prefers them over treating `ស្មីត` as a single unknown block.
2.  **Result**: `លោក` | ` ` | `ចន` | ` ` | `ស្មី` | `ត`

### Example 3: Invalid Single Consonant Penalty
**Input**: `ការងារ` (Job)
*   Dictionary has `ការងារ` as a compound word.
*   The algorithm prefers the longest match `ការងារ` over splitting into `ការ` | `ងារ` or smaller parts.
*   **Result**: `ការងារ`

**Input**: `តាប៉ិ` (Old man, slang/informal)
*   `តា` (Known "Grandpa").
*   `ប៉ិ` (Unknown).
*   **Result**: `តា` | `ប៉ិ` (Correctly keeps known word, flags rest as unknown).

## 5. Comparison with khmernltk

We compared the performance and output of `KhmerSegmenter` against `khmernltk` using a complex sentence from a folktale.

### Finding Unknown Words

You can analyze the segmentation results to find words that were not in the dictionary (potential new words or names):

```bash
python scripts/find_unknown_words.py --input segmentation_results.txt
```

This will generate `data/unknown_words_from_results.txt` showing the unknown words, their frequency, and **context** (2 words before and after) to help you decide if they should be added to the dictionary.

## 6. Benchmark & Performance Comparison

We compared `KhmerSegmenter` against `khmernltk` using real-world complex text:

|Feature|khmernltk (Python)|KhmerSegmenter (Python)|KhmerSegmenter (C Port)|
|:---|:---|:---|:---|
|**Cold Start (Load)**|~1.83s|~0.30s (6x Faster)|**< 0.05s** (Instant)|
|**Memory Usage**|~113.6 MB|~21.6 MB (5x Leaner)|**~14 MB** (Lowest)|
|**Execution Speed (Seq)**|~5.77ms / call|~5.77ms / call (Baseline)|**~3.91ms / call** (1.5x Faster)|
|**Concurrent (10 Workers)**|~318 calls / sec (GIL)|~447 calls / sec (GIL)|**~3235 calls / sec** (7x Faster)|
|**Concurrent Memory Delta**|~12.1 MB|~19.0 MB|**~1.0 MB** (Efficient)|
|**Complex Input**|Splits numbers/acronyms|Correctly Groups (Rules)|**Correctly Groups**|
|**Characteristics**|ML/Rule Hybrid|Pure Logic (Python)|**Pure Logic (Native)**|

### Performance & Portability Analysis

#### 1. Concurrency & Threading
Benchmarks run with `10 workers` using a `ThreadPoolExecutor` show that `KhmerSegmenter` achieves **~447 calls/sec** vs `khmernltk`'s **~318 calls/sec**.

*   **Python Limitations (GIL)**: In Python, concurrent performance is restricted by the **Global Interpreter Lock (GIL)**. This limits true parallelism.
*   **C Port Advantage**: The C port, free from the GIL, achieves **~3235 calls/sec** (over **7x faster** than Python concurrent). This demonstrates linear scaling: adding more CPU cores directly translates to higher throughput, making it ideal for high-load server environments.

#### 2. Portability (Universal Compatibility)
*   **KhmerSegmenter**: **Pure Python**. Requires **Zero** external dependencies beyond the standard library. It runs anywhere Python runs (Lambda, Edge devices, Windows/Linux/Mac) without compilation.
*   **Language Agnostic**: The core algorithm consists of standard loops, array lookups, and arithmetic. It can be easily ported to **ANY** programming language (JavaScript, Rust, Go, Java, etc.).
*   **Web & Edge Ready**: Perfect for client-side JavaScript execution (via WASM/Pyodide) or edge computing where low latency and small binary size are crucial.

#### 3. Cold Start
`KhmerSegmenter` initializes in **~0.30s**, whereas `khmernltk` takes **~1.8s+** to load its model. This makes `KhmerSegmenter` ideal for "Serverless" functions where startup latency is a primary billing and UX concern.

### Real-World Complex Sentence Example

**Input:**
> "ក្រុមហ៊ុនទទួលបានប្រាក់ចំណូល ១ ០០០ ០០០ ដុល្លារក្នុងឆ្នាំនេះ ខណៈដែលតម្លៃភាគហ៊ុនកើនឡើង ៥% ស្មើនឹង 50.00$។ លោក ទេព សុវិចិត្រ នាយកប្រតិបត្តិដែលបញ្ចប់ការសិក្សាពីសាកលវិទ្យាល័យភូមិន្ទភ្នំពេញ (ស.ភ.ភ.ព.) បានថ្លែងថា ភាពជោគជ័យផ្នែកហិរញ្ញវត្ថុនាឆ្នាំនេះ គឺជាសក្ខីភាពនៃកិច្ចខិតខំប្រឹងប្រែងរបស់ក្រុមការងារទាំងមូល និងការជឿទុកចិត្តពីសំណាក់វិនិយោគិន។"

**khmernltk Result (v1.5):**
> `ក្រុមហ៊ុន` | `ទទួលបាន` | `ប្រាក់` | `ចំណូល` | ` ` | `១` | ` ` | `០០០` | ` ` | `០០០` | ` ` | `ដុល្លារ` | `ក្នុង` | `ឆ្នាំ` | `នេះ` | ` ` | `ខណៈ` | `ដែល` | `តម្លៃ` | `ភាគហ៊ុន` | `កើនឡើង` | ` ` | `៥%` | ` ` | `ស្មើនឹង` | ` ` | `50.` | `00$` | `។` | ` ` | `លោក` | ` ` | `ទេព` | ` ` | `សុវិចិត្រ` | ` ` | `នាយក` | `ប្រតិបត្តិ` | `ដែល` | `បញ្ចប់` | `ការសិក្សា` | `ពី` | `សាកលវិទ្យាល័យ` | `ភូមិន្ទ` | `ភ្នំពេញ` | ` ` | `(` | `ស.` | `ភ.` | `ភ.` | `ព.` | `)` | ` ` | `បាន` | `ថ្លែង` | `ថា` | ` ` | `ភាពជោគជ័យ` | `ផ្នែក` | `ហិរញ្ញវត្ថុ` | `នា` | `ឆ្នាំ` | `នេះ` | ` ` | `គឺជា` | `សក្ខីភាព` | `នៃ` | `កិច្ច` | `ខិតខំ` | `ប្រឹងប្រែង` | `របស់` | `ក្រុមការងារ` | `ទាំងមូល` | ` ` | `និង` | `ការជឿទុកចិត្ត` | `ពីសំណាក់` | `វិនិយោគិន` | `។`

**KhmerSegmenter Result (Ours):**
> `ក្រុមហ៊ុន` | `ទទួល` | `បាន` | `ប្រាក់ចំណូល` | ` ` | `១ ០០០ ០០០` | ` ` | `ដុល្លារ` | `ក្នុង` | `ឆ្នាំ` | `នេះ` | ` ` | `ខណៈ` | `ដែល` | `តម្លៃ` | `ភាគហ៊ុន` | `កើនឡើង` | ` ` | `៥` | `%` | ` ` | `ស្មើនឹង` | ` ` | `50.00` | `$` | `។` | ` ` | `លោក` | ` ` | `ទេព` | ` ` | `សុវិចិត្រ` | ` ` | `នាយក` | `ប្រតិបត្តិ` | `ដែល` | `បញ្ចប់` | `ការសិក្សា` | `ពី` | `សាកលវិទ្យាល័យ` | `ភូមិន្ទ` | `ភ្នំពេញ` | ` ` | `(` | `ស.ភ.ភ.ព.` | `)` | ` ` | `បាន` | `ថ្លែង` | `ថា` | ` ` | `ភាព` | `ជោគជ័យ` | `ផ្នែក` | `ហិរញ្ញវត្ថុ` | `នា` | `ឆ្នាំ` | `នេះ` | ` ` | `គឺជា` | `សក្ខីភាព` | `នៃ` | `កិច្ច` | `ខិតខំ` | `ប្រឹងប្រែង` | `របស់` | `ក្រុមការងារ` | `ទាំងមូល` | ` ` | `និង` | `ការ` | `ជឿ` | `ទុកចិត្ត` | `ពីសំណាក់` | `វិនិយោគិន` | `។`

**Key Differences:**
1.  **Numbers**: `khmernltk` splits `១ ០០០ ០០០` into 5 tokens. `KhmerSegmenter` keeps it as **one**.
2.  **Acronyms**: `khmernltk` destroys `(ស.ភ.ភ.ព.)` into multiple tokens. `KhmerSegmenter` keeps it as **one**.
3.  **Dictionary Adherence**: `KhmerSegmenter` strictly adheres to the dictionary. For example, it correctly splits `ភាគហ៊ុន` into `ភាគ` | `ហ៊ុន` if `ភាគហ៊ុន` isn't in the loaded dictionary but the parts are (or vice versa depending on dictionary state). *Note: Benchmarks reflect the current state of `khmer_dictionary_words.txt`. As you add words like `ភាគហ៊ុន`, the segmenter will automatically group them.*

### Portability & Universal Compatibility
Because `KhmerSegmenter` relies on **pure mathematical logic (Viterbi Algorithm)** and simple string matching:
*   **Language Agnostic**: The core algorithm consists of standard loops, array lookups, and arithmetic operations. It can be easily ported to **ANY** programming language (JavaScript, Go, Rust, Java, C#, C++, etc.) without dependency hell.
*   **CPU Efficient**: It runs efficiently on standard CPUs without needing GPUs or heavy matrix multiplication libraries (like NumPy/TensorFlow).
*   **Zero Dependencies**: Unlike ML-based solutions that require specific runtime environments (e.g. `scikit-learn`, `libpython`), this logic is self-contained and highly embeddable.
*   **Web & Edge Ready**: Perfect for client-side JavaScript execution (via WASM or direct port) or edge computing where low latency and small binary size are crucial.

## 7. Testing & Verification

You can verify the segmentation logic using the `scripts/test_viterbi.py` script. This script supports both single-case regression testing and batch processing of a corpus.

### Run Standard Test Cases
```bash
python scripts/test_viterbi.py
```

### Batch Process a Corpus
To test against a file and see the output:
```bash
python scripts/test_viterbi.py --source data/khmer_folktales_extracted.txt --limit 500
```
This will generate `segmentation_results.txt`.

## 8. License

MIT License

Copyright (c) 2026 Sovichea Tep

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

You are free to use, modify, and distribute this software, but you **must acknowledge usage** by retaining the copyright notice and license in your copies.

## 9. Acknowledgements

*   **[khmernltk](https://github.com/VietHoang1512/khmer-nltk)**: Used for initial corpus tokenization and baseline frequency generation.
*   **[sovichet](https://github.com/sovichet)**: For providing the [Khmer Folktales Corpus](https://github.com/sovichet) and Dictionary resources.
