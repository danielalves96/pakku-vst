# Changelog

All notable changes to Pakku are recorded here.
This project follows [Semantic Versioning](https://semver.org).

## [1.0.0] — 2026-09-04

First public release.

### Added
- Three-band transient shaping over a Linkwitz–Riley 4th-order crossover, with
  allpass compensation so the bands sum flat (0.04 dB deviation).
- Single-band mode: the spectrum steps aside and the waveform takes the whole
  display.
- Ceiling stage with two characters — a limiter with 5 ms lookahead, or a soft
  clipper with the knee below the threshold — running 4× oversampled with a
  linear-phase filter.
- Tone section (Presence, Air) and parallel compression (NYC).
- Latency-compensated dry path, so partial Mix does not comb-filter.
- Threshold draggable straight off the waveform, two-way with the knob.
- 31 factory presets, mirrored to disk in `Factory/` and restorable from the
  copies compiled into the binary.
- `.pkku` preset container: `PKKU` signature, format version, and a binary
  `ValueTree` holding real parameter values rather than normalised ones.
- Three interface sizes, stored with the session state.
- Settings panel: interface size, preset folder shortcuts, restore defaults,
  version and reported latency.
- macOS `.pkg` and Windows Inno Setup installers.

### Notes
- Preset files store parameter values in linear scale. Nothing in the loader
  branches on the version string, so the version number stays a version and
  never changes how a file is read.
