"""Extract ORIGINAL lossless shader tokens, without copying textures/model data.
Usage: python3 tests/extract_native_captures.py /path/to/log-directory /path/to/output
"""
from pathlib import Path
import re, struct, sys
source, target = map(Path, sys.argv[1:])
target.mkdir(parents=True, exist_ok=True)
for file in sorted(source.glob('PSXFX.SA_shader_*.txt')):
    original = file.read_text().split('TOKENS (lossless):', 1)[1].split('=== ADAPTED', 1)[0]
    tokens = [int(value, 16) for value in re.findall(r'\b[0-9a-f]{8}\b', original)]
    assert tokens and tokens[0] == 0xfffe0201 and tokens[-1] == 0xffff
    (target / (file.stem + '.bin')).write_bytes(struct.pack('<' + 'I' * len(tokens), *tokens))
