# Khmer Segmenter (C Port)

A high-performance C port of the Khmer Segmenter, capable of segmenting Dictionary-based and Rule-based Khmer text. This port is built using the properties of the [Zig Build System](https://ziglang.org/) for easy cross-compilation and dependency management.

## Prerequisites

### Install Zig Compiler
1.  Download Zig from [https://ziglang.org/download/](https://ziglang.org/download/).
    -   *Recommended version*: **0.13.0** or latest stable.
2.  Extract the archive to a location (e.g., `C:\zig` or `/usr/local/zig`).
3.  Add the folder containing `zig.exe` (or `zig` binary) to your system **PATH**.
4.  Verify installation:
    ```bash
    zig version
    ```

## Building

Navigate to the `c_port` directory:
```bash
cd c_port
```

### 1. Debug Build (Default)
Fast compilation, includes debug symbols and runtime safety checks. Slower execution speed.
```bash
zig build
```
*Output*: `zig-out/bin/khmer_segmenter.exe`

### 2. Release Build (Optimized)
Fully optimized for speed. **Recommended for production or benchmarking.**
```bash
zig build -Doptimize=ReleaseFast
```
*Output*: `zig-out/bin/khmer_segmenter.exe`

## Usage

The executable supports direct text input, file processing, and benchmarking.

### Segment Raw Text
Use custom separator (default is specialized pipe/space if not specified, but the CLI wrapper typically joins with ` | `).
```bash
# Windows
.\zig-out\bin\khmer_segmenter.exe "ខ្ញុំស្រឡាញ់ប្រទេសកម្ពុជា"

# Output: ខ្ញុំ | ស្រឡាញ់ | ប្រទេស | កម្ពុជា
```

### Batch File Processing
Process a file line-by-line and print segmented output to stdout.
```bash
.\zig-out\bin\khmer_segmenter.exe --file input.txt > output.txt
```

### Run Benchmark
Run the built-in performance suite (Sequential and Multi-threaded).
```bash
# Run with 4 threads
.\zig-out\bin\khmer_segmenter.exe --benchmark --threads 4
```

## Performance Comparison

Comparing segmentation speed on a long text paragraph (~935 characters):

| Version | Build Type | Time per Call | Throughput | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Python** | N/A | ~5.77 ms | ~173 calls/s | Baseline logic |
| **C Port** | Debug | ~8.56 ms | ~116 calls/s | Includes safety checks & unoptimized code |
| **C Port** | Release (Seq) | ~3.91 ms | ~255 calls/s | Single Thread (~1.5x Faster than Python) |
| **C Port** | **Release (10 Threads)** | **~0.31 ms*** | **~3235 calls/s** | **Massive Throughput scaling** |

*> * Effective time per call under load (1/Throughput).*
*> Note: Benchmarks run on standard consumer hardware. Multi-threaded throughput scales linearly with core count.*
