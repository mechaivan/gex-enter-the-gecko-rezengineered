# GEX objdiff Configuration

This directory contains the objdiff configuration for tracking Gex: Enter the Gecko decompilation progress on [bytematch.dev](https://bytematch.dev/mechaivan/gex-enter-the-gecko-rezengineered).

## Files

| File | Purpose |
|------|---------|
| `target.toml` | Target executable metadata (hashes, PE headers, toolchain) |
| `match-units.toml` | Build definitions and matching units (functions to match) |
| `objdiff.yml` | Root objdiff configuration |

## Target Binary

**Gex: Enter the Gecko — EU v1.00.000 (Glide)**
- File: `GEX3D.EXE`
- MD5: `692b12825003417cc4a5a9d4db13eebe`
- SHA256: `d0932549613f9c08fb895c5f8a6461ce044e1863b4cba42da4715c2b41e73886`
- PE32, i386, Glide renderer
- Timestamp: 1998-05-19T12:49:46Z

## Current Units

| Unit | Address | Size | Status | Description |
|------|---------|------|--------|-------------|
| `timing_update` | `0x0043E1BA` | 1090 bytes | Planned | Game loop update: `elapsed/16.666` → `[obj+0x10C]`, `+0x104++`, swap queue |

## Adding New Units

1. Add build definition in `[builds.<name>]` section
2. Add unit in `[units.<name>]` section with:
   - `build` = build name
   - `symbol` = function name (or `FUN_XXXXXXXX`)
   - `target_address` = virtual address in exe
   - `size` = function size in bytes
   - `boundary_evidence` = link to analysis doc
   - `relocations` = list of relocations (REL32/DIR32)

## Generating Report Locally

```bash
# Install objdiff-cli
pip install objdiff-cli

# Generate report
objdiff-cli report generate --config config/target.toml --units config/match-units.toml --output build/EU_report.json --version EU

# View report
python -c "import json; r=json.load(open('build/EU_report.json')); print(f'Matched: {r.get(\"matched_functions\",0)}/{r.get(\"total_functions\",0)}')"
```

## CI Integration

The `.github/workflows/byte-match.yml` workflow:
1. Runs on push to `dev/gex`
2. Generates `EU_report.json` via `objdiff-cli`
3. Uploads as artifact `EU_report`
4. bytematch.dev polls every 15 min (or via GitHub App webhook)

## References

- [bytematch.dev Getting Started](https://bytematch.dev/docs/getting-started)
- [objdiff Documentation](https://objdiff.encounter.dk/)
- [dx-ball reference config](https://github.com/N0zoM1z0/dx-ball/tree/main/config)