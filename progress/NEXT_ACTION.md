# Next action
1. Push branch and run `.github/workflows/build.yml` to produce Windows NSIS/ZIP, macOS DMGs and the Linux AppImage (Q1).
2. Execute the hardware validation checklist with grandMA3 and MagicQ (Q2, Q6) and record results in TEST_RESULTS.md.
3. Benchmark on the target show machine (`lumaflow-cli benchmark --width 1920 --height 1080 -o bench.json`) (Q3).
Resume point: all source, tests, docs and media are committed; rebuild with `cmake -S . -B build -G Ninja && cmake --build build`.
