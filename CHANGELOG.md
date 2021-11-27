# Changelog

## [Unreleased]

## [v0.3.1] - 2021-11-27

- Add interface for changing ggwave's internal logging ([#52](https://github.com/ggerganov/ggwave/pull/52), [#55](https://github.com/ggerganov/ggwave/pull/55))
- Fix out-of-bounds access in `ggwave_decode` ([#53](https://github.com/ggerganov/ggwave/pull/53))
- Add C interface for selecting Rx protocols ([#60](https://github.com/ggerganov/ggwave/pull/60))

## [v0.3.0] - 2021-07-03

- Resampling fixes
- Add `soundMarkerThreshold` parameter ([f4fb02d](https://github.com/ggerganov/ggwave/commit/f4fb02d5d4cfd6c1021d73b55a0e52ac9d3dbdfa))
- Sampling rates are now consistently represented as float instead of int
- Add option to query the generated tones ([ba87a65](https://github.com/ggerganov/ggwave/commit/ba87a651e3e27ce3fa9a85d53ca988a0cedd2e46))
- Fix python build on Windows ([d73b184](https://github.com/ggerganov/ggwave/commit/d73b18426bf0df0e610c31c948e0ddf9a0784073))

## [v0.2.0] - 2021-02-20

- Supported sampling rates: 6kHz - 96kHz
- Variable-length payloads
- Fixed-length payloads (no sound markers emitted)
- Reed-Solomon based ECC
- Ultrasound support

[unreleased]: https://github.com/ggerganov/ggwave/compare/ggwave-v0.3.1...HEAD
[v0.3.1]: https://github.com/ggerganov/ggwave/releases/tag/ggwave-v0.3.1
[v0.3.0]: https://github.com/ggerganov/ggwave/releases/tag/ggwave-v0.3.0
[v0.2.0]: https://github.com/ggerganov/ggwave/releases/tag/ggwave-v0.2.0
