# Clean Mic 3.17.40: root cause and verified fix

2026-10-27. Functional integration is verified. The official executable, account/session, authentication, and licensing were preserved. See [VERIFICATION.md](VERIFICATION.md) for the complete measurements and remaining acoustic limitations.

## Original failure

Clean Mic returned `loadVoice -93` with `Error during string conversion NoReverb to float`. The catalog identifies voice `df6454f1-8eb2-4092-9ccc-fb51219f6291`, version 9.0.0, asset `clean.xml`. The catalog identity was recorded locally; private asset metadata is not distributed here. The cached asset format was not established.

Bounded runtime inspection of the parsed resource established two dynamic mixers. `NoReverb` computes `1 - Reverb Volume`, where Reverb Volume defaults to .02. The inner MixN volume references those two parameters, initially [.98,.02]. `NaturalVoice` computes `1 - Mix`, with Mix initially 1; the outer MixN initially uses [0,1]. Hidden parameters were not changed or frozen to constants. The resource also contains unprefixed references and explicit @ references, so adding @ indiscriminately would not be justified.

## Proven root cause

Local disassembly analysis identified the original XML resolution path:

- Loader 0x1400917f0 splits vector attributes by comma and scans each token with `%lf`, returning at 0x1400928c8.
- Wrapper 0x140b19e90 calls `__stdio_common_vsscanf` at IAT RVA 0xe547e0 with options 2, returning at RVA 0xb19eda.
- Scanner result zero selects native lookup 0x14017f880, per-element link creation 0x1401b1fb0, dependency registration 0x1401a6440, and value evaluation 0x1401b12b0.
- Any nonzero scanner result preserves the token as a literal. Vector setter 0x140217750 then reaches float parser 0x1402182a0, which rejects the identifier.
- MixN vtable is 0x14260e368; setter 0x1402172a0 updates volume parameter index 2. Constructor: 0x140216190.

`compat/clean_mic_scanf_probe.c`, run under the same Wine/prefix, proved that NoReverb, NoSpace, and NaturalVoice return -1 with unchanged numeric destination. Other nonnumeric identifiers return zero. Numbers and nan/inf return 1; empty input returns -1. Options 0 and 2 were tested. The standalone probe source is included; its raw local log is not distributed.

Wine's attempted nan recognition causes these N-prefixed names to return EOF rather than a matching failure. The application therefore sends an identifier to the numeric parser. The SDK already implements all required dynamic resolution and linking.

## Narrow compatibility repair

`compat/clean_mic_fix.c` changes only the main application's scanf IAT slot in memory. It normalizes -1 to zero only for the exact studied build and call path: PE timestamp 0x6adb566e, SizeOfImage 0x3246000, expected complete call-site bytes, options 2, format `%lf`, length SIZE_MAX, null locale, wrapper return RVA 0xb19eda, XML stack return RVA 0x928c8, and a bounded N-prefixed identifier.

The original SDK then resolves, evaluates, and links the real parameters. No XML, parameter values, account data, license, or signed executable is patched. The launcher enables the fix with `VMCOMPAT_FIX_FLOAT_REFS=1`. The source and standalone probe compiled with the private MinGW toolchain. The optional `compat/clean_mic_probe.c` diagnostic detour remains disabled.

## Integration verified

The library populated during a normal launch without diagnostic CDP flags. Its earlier persistent placeholders were not reproduced; their exact cause is unknown. Clean Mic was selected through its normal available library tile, switched to Custom Pitch, and selected again.

Actual Clean Mic loads at application-log timestamps 04:17:07 and 04:24:37 each produced NoReverb/NaturalVoice correction events without the former conversion error. Read-only `clean_mic_inspect_links.py` inspection on both loads found the original SDK element links and mixer values [.980000019,.0199999996] and [0,1]. No general memory dump was saved.

Calibrated generated tones and speech produced real Clean Mic DSP output with Hear Myself off. Public Bass changes measurably changed that output. Discord's local test returned the corresponding Clean Mic and Custom Pitch content on headphones-rear; the Creative front monitor was silent. Controls and physical input were restored. See [VERIFICATION.md](VERIFICATION.md) and its linked evidence for exact results, reproduction commands, and practical limits.
