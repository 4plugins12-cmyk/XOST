#!/usr/bin/env python3
"""Loudness-match presets: runs the test harness and moves each preset's master_vol toward a target
short-term level (loudest 400 ms window), then regenerates Presets.cpp. Run it 2-3 times."""
import re, subprocess, json, os, sys
here = os.path.dirname(os.path.abspath(__file__)); root = os.path.dirname(here)
target = float(sys.argv[1]) if len(sys.argv) > 1 else -15.0
gfile = os.path.join(here, 'gains.json')
gains = json.load(open(gfile)) if os.path.exists(gfile) else {}
r = subprocess.run(['cmake', '--build', 'build', '--target', 'XOSTTest'], cwd=root, capture_output=True, text=True)
if r.returncode: print(r.stdout[-3000:]); sys.exit(1)
out = subprocess.run(['xvfb-run', '-a', './build/XOSTTest_artefacts/Release/XOSTTest', '/tmp/claude-0/xst'],
                     cwd=root, capture_output=True, text=True).stdout
open('/tmp/claude-0/calib_last.log', 'w').write(out)
errs, limited = [], []
for line in out.splitlines():
    m = re.match(r'\s*(?:!!)?\s*\d+ (\S+)\s+(.+?)\s+peak\s+(\S+) dB\s+rms\s+(\S+) dB\s+st\s+(\S+) dB', line)
    if not m: continue
    name, st = m.group(2).strip(), float(m.group(5))
    if name == 'Init': continue
    cur = gains.get(name, -6.0)
    want = cur + (target - st)
    new = max(-30.0, min(26.0, want))
    if want > 26.0: limited.append((name, round(want - 6.0, 1)))
    gains[name] = round(new, 1)
    errs.append(abs(target - st))
json.dump(gains, open(gfile, 'w'), indent=1, sort_keys=True)
print('max err %.1f dB, mean %.2f, %d presets' % (max(errs), sum(errs) / len(errs), len(errs)))
if limited: print('needs more gain than master allows:', limited)
subprocess.run(['python3', os.path.join(here, 'gen_presets.py')], check=True)
