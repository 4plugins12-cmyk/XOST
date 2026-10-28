#!/usr/bin/env python3
"""Distinctness check: spectral + temporal fingerprints of every preset render, nearest-neighbour distances."""
import numpy as np, glob, os, re, sys, json
from scipy.io import wavfile
d = sys.argv[1] if len(sys.argv) > 1 else '/tmp/claude-0/xt'
thr = float(sys.argv[2]) if len(sys.argv) > 2 else 0.0
files = sorted(f for f in glob.glob(os.path.join(d, '[0-9][0-9][0-9][0-9]_*.wav')))
names, feats = [], []
edges = np.geomspace(40, 18000, 33)
for f in files:
    sr, x = wavfile.read(f)
    x = x.astype(np.float64); x /= (np.abs(x).max() + 1e-9)
    m = x.mean(1); side = (x[:, 0] - x[:, 1]) / 2
    parts = [m[: int(sr * 3.2)], m[int(sr * 3.2): int(sr * 4.8)], m[int(sr * 4.8):]]
    fv = []
    for seg in parts[:2]:
        spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2
        fr = np.fft.rfftfreq(len(seg), 1 / sr)
        bands = np.array([spec[(fr >= a) & (fr < b)].sum() for a, b in zip(edges[:-1], edges[1:])]) + 1e-12
        lb = 10 * np.log10(bands); lb -= lb.max()
        fv.append(np.maximum(lb, -60) / 60.0 * 2.0)
    hop = sr // 20
    env = np.array([np.sqrt(np.mean(m[i:i + hop] ** 2)) for i in range(0, len(m) - hop, hop)])
    env = 20 * np.log10(env + 1e-6); env = np.maximum(env - env.max(), -60) / 60.0
    fv.append(env * 1.5)
    fv.append([np.sqrt(np.mean(side ** 2)) / (np.sqrt(np.mean(m ** 2)) + 1e-9) * 2.0])
    names.append(re.sub(r'^\d+_', '', os.path.basename(f))[:-4]); feats.append(np.concatenate([np.ravel(a) for a in fv]))
F = np.array(feats)
D = np.sqrt(((F[:, None, :] - F[None, :, :]) ** 2).sum(-1)); np.fill_diagonal(D, 1e9)
nn = D.min(1); j = D.argmin(1)
order = np.argsort(nn)
print('presets %d  nearest-neighbour distance: min %.2f  median %.2f' % (len(names), nn.min(), np.median(nn)))
for i in order[:25]: print('  %.2f  %-28s ~ %s' % (nn[i], names[i], names[j[i]]))
if thr > 0:
    rej = sorted({names[i] for i in range(len(names)) if nn[i] < thr and i > j[i]})
    print('rejecting', len(rej)); json.dump([r.replace('_', ' ') for r in rej], open(os.path.join(os.path.dirname(__file__), 'reject_new.json'), 'w'))
