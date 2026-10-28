"""Shared building blocks for XO's preset generators (synth presets, Matrix bank, Minirack bank)."""
import random, json, os, math
here = os.path.dirname(os.path.abspath(__file__))
root = os.path.dirname(here)

T = dict(basic=0, pwm=1, sweep=2, bell=3, glass=4, vocal=5, digital=6, hollow=7, bubble=8, supersaw=9, organ=10,
         growl=11, chip=12, flute=13, pluck=14, sinestack=15)
WARP = dict(off=0, sync=1, bend=2, mirror=3, pwm=4, quant=5, selffm=6)
OMOD = dict(off=0, fm=1, pm=2, ring=3, am=4, hsync=5)
FLT = dict(lp12=0, lp24=1, ladder=2, hp12=3, hp24=4, bp=5, notch=6, peak=7, comb=8, formant=9)
LFO = dict(sine=0, tri=1, sawup=2, sawdn=3, square=4, sh=5, smooth=6, bounce=7, stairs=8, pump=9)
DIV = {'8bar': 0, '4bar': 1, '2bar': 2, '1bar': 3, '1/2': 4, '1/2T': 5, '1/4.': 6, '1/4': 7, '1/4T': 8, '1/8.': 9, '1/8': 10,
       '1/8T': 11, '1/16.': 12, '1/16': 13, '1/16T': 14, '1/32': 15, '1/64': 16}
ARP = dict(up=0, down=1, updown=2, random=3, played=4, chord=5, seq=6)
DIST = dict(overdrive=0, tube=1, hard=2, crush=3, fold=4, rect=5)
SRC = dict(env1=1, env2=2, env3=3, lfo1=4, lfo2=5, lfo3=6, lfo4=7, vel=8, note=9, press=10, wheel=11, bend=12, slide=13,
           rnd=14, m1=15, m2=16, m3=17, m4=18, m5=19, m6=20, m7=21, m8=22)
_d = ["none", "apitch", "apos", "alevel", "apan", "adetune", "awarp", "amod", "bpitch", "bpos", "blevel", "bpan", "bdetune",
      "bwarp", "bmod", "cpitch", "cpos", "clevel", "cpan", "cdetune", "cwarp", "cmod", "sub", "noise", "noisecol", "smplevel",
      "smppos", "grainsize", "cut1", "res1", "drive1", "cut2", "res2", "shpdrive", "amp", "pan", "lfo1rate", "lfo2rate",
      "lfo3rate", "lfo4rate", "distdrive", "distmix", "chorusmix", "phasermix", "delaymix", "delayfb", "revmix", "revsize", "master"]
DST = {k: i for i, k in enumerate(_d)}
CHORD = dict(off=0, minor=1, major=2, min7=3, maj7=4, min9=5, maj9=6, madd9=7, sus2=8, sus4=9, power=10, oct=11, dim=12, phryg=13)
RACK = dict(empty=0, lfo=1, sh=2, drunk=3, turing=4, chaos=5, seq=6, bounce=7, follower=8)
MXLEN = {'1/4': 0, '1/2': 1, '1bar': 2, '2bar': 3, '4bar': 4}

DEFAULTS = {}
for line in open(os.path.join(here, 'defaults.txt')):
    k, _, v = line.strip().partition('=')
    if k: DEFAULTS[k] = float(v)

R = random.Random(1)
def U(a, b, nd=3): return round(R.uniform(a, b), nd)
def C(*xs): return R.choice(xs)
def P(p): return R.random() < p

def num(x):
    x = round(float(x), 4)
    return str(int(x)) if x == int(x) else repr(x)

# ---------------------------------------------------------------- Matrix lane shapes (lists of (x, y, curve))
def lane_steps(vals):
    n = len(vals); out = []
    for k, y in enumerate(vals):
        out += [(k / n, y, 0.0), ((k + 1) / n - 0.002, y, 0.0)]
    return out

def lane_gate(n, duty=0.5, pattern=None, low=0.0):
    out = []
    for k in range(n):
        x = k / n
        on = 1.0 if pattern is None else pattern[k % len(pattern)]
        hi = low + (1.0 - low) * on
        out += [(x, hi, 0.0), (x + duty / n, hi, 0.0), (x + duty / n + 0.002, low, 0.0), (x + 1 / n - 0.002, low, 0.0)]
    return out

def lane_pump(n, depth=1.0, curve=-0.6):
    out = []
    for k in range(n):
        x = k / n
        out += [(x, 1 - depth, 0.0), (x + 0.15 / n, 1 - depth * 0.9, curve), (x + 0.8 / n, 1.0, 0.0)]
    return out

def lane_sine(cycles=1, lo=0.0, hi=1.0, pts=16):
    return [(k / pts, lo + (hi - lo) * (0.5 + 0.5 * math.sin(2 * math.pi * cycles * k / pts)), 0.0) for k in range(pts)]

def lane_ramp(up=True, curve=0.0, lo=0.0, hi=1.0):
    return [(0.0, lo if up else hi, curve), (0.995, hi if up else lo, 0.0)]

def lane_random(n, lo=0.0, hi=1.0, rng=None):
    rng = rng or R
    return lane_steps([lo + (hi - lo) * rng.random() for _ in range(n)])

def lane_str(pts):
    pts = sorted([(min(0.9999, max(0.0, x)), min(1.0, max(0.0, y)), c) for x, y, c in pts])
    return ';'.join('%s,%s,%s' % (num(x), num(y), num(c)) for x, y, c in pts)

# ---------------------------------------------------------------- the patch object
class Patch:
    def __init__(s, name, cat, tags, desc):
        s.name, s.cat, s.tags, s.desc = name, cat, tags, desc
        s.v = {'oa_on': 0}
        s.slots = []
        s.lanes = {}

    def set(s, **kw):
        s.v.update(kw); return s

    def osc(s, o, table, pos=0.0, oct=0, semi=0, fine=0.0, level=0.75, uni=1, det=0.2, width=0.7, rand=1.0,
            warp='off', warpamt=0.0, mod='off', modfrom=None, modamt=0.0, route=0, pan=0.0):
        p = 'o' + 'abc'[o] + '_'
        s.v.update({p + 'on': 1, p + 'table': T[table], p + 'pos': pos, p + 'oct': oct, p + 'semi': semi, p + 'fine': fine,
                    p + 'level': level, p + 'uni': uni, p + 'detune': det, p + 'width': width, p + 'rand': rand,
                    p + 'warp': WARP[warp], p + 'warpamt': warpamt, p + 'mod': OMOD[mod], p + 'modamt': modamt,
                    p + 'route': route, p + 'pan': pan})
        if modfrom is not None: s.v[p + 'modfrom'] = modfrom
        return s

    def env(s, e, a=1, d=400, sus=0.0, r=300, h=0, dl=0, curve=0.3):
        p = 'e%d_' % e
        s.v.update({p + 'attack': a, p + 'decay': d, p + 'sustain': sus, p + 'release': r, p + 'hold': h, p + 'delay': dl, p + 'curve': curve})
        return s

    def lfo(s, l, shape='sine', div='1/8', sync=1, rate=2.0, mode=1, phase=0.0, smooth=0.0):
        p = 'l%d_' % l
        s.v.update({p + 'shape': LFO[shape], p + 'div': DIV[div], p + 'sync': sync, p + 'rate': rate, p + 'mode': mode,
                    p + 'phase': phase, p + 'smooth': smooth})
        return s

    def flt(s, f, typ='lp24', cut=8000, reso=0.15, drive=0.0, key=0.0, env=0.0, mix=1.0):
        p = 'f%d_' % f
        s.v.update({p + 'on': 1, p + 'type': FLT[typ], p + 'cutoff': cut, p + 'reso': reso, p + 'drive': drive,
                    p + 'key': key, p + 'env': env, p + 'mix': mix})
        return s

    def mod(s, src, dst, amt):
        if len(s.slots) < 16: s.slots.append((SRC[src], DST[dst], amt))
        return s

    def fx(s, which, **kw):
        s.v[which + '_on'] = 1
        for k, x in kw.items(): s.v[which + '_' + k] = x
        return s

    # Matrix: lanes 0 vol, 1 flt, 2 pan, 3 time
    def matrix(s, lanes, mix=1.0, smooth=None, lens=None, depths=None, flt_type=0, flt_reso=0.3, time_range=2):
        s.v.update(mx_on=1, mx_mix=mix, mx_flt_type=flt_type, mx_flt_reso=flt_reso, mx_time_range=time_range)
        if smooth is not None: s.v['mx_smooth'] = smooth
        names = ['vol', 'flt', 'pan', 'time']
        for i in range(4):
            on = i in lanes
            s.v['mx_%s_on' % names[i]] = 1 if on else 0
            if on:
                s.lanes[i] = lanes[i]
                s.v['mx_%s_len' % names[i]] = MXLEN[(lens or {}).get(i, '1bar')]
                s.v['mx_%s_depth' % names[i]] = (depths or {}).get(i, 1.0)
        return s

    def rack(s, modules, depth=1.0):
        s.v.update(rk_on=1, rk_depth=depth)
        for i, m in enumerate(modules[:6]):
            k = 'rk%d_' % (i + 1)
            s.v[k + 'type'] = RACK[m['type']]
            s.v[k + 'sync'] = m.get('sync', 1)
            if 'rate' in m: s.v[k + 'rate'] = m['rate']
            if 'div' in m: s.v[k + 'div'] = DIV[m['div']]
            s.v[k + 'a'] = m.get('a', 0.0); s.v[k + 'b'] = m.get('b', 0.3)
            s.v[k + 'dst1'] = DST[m['dst']]; s.v[k + 'amt1'] = m['amt']
            if 'dst2' in m: s.v[k + 'dst2'] = DST[m['dst2']]; s.v[k + 'amt2'] = m['amt2']
        return s

    def values(s, gains):
        v = dict(s.v)
        for i, (a, b, c) in enumerate(s.slots):
            v['mm%d_src' % (i + 1)] = a; v['mm%d_dst' % (i + 1)] = b; v['mm%d_amt' % (i + 1)] = round(c, 3)
        if s.name in gains:
            g = gains[s.name]
            v['master_vol'] = min(6.0, g)
            if g > 6.0:
                if not v.get('comp_on'): v.update(comp_on=1, comp_ratio=1.0, comp_gain=0.0, comp_mix=1.0)
                v['comp_gain'] = round(min(24.0, v.get('comp_gain', 4.0) + g - 6.0), 1)
        return v

    def line(s, gains):
        v = s.values(gains)
        toks = ['%s=%s' % (k, num(x)) for k, x in sorted(v.items()) if k not in DEFAULTS or abs(DEFAULTS[k] - float(x)) > 1e-6]
        for i, pts in sorted(s.lanes.items()): toks.append('mx%d=%s' % (i, lane_str(pts)))
        for f in (s.name, s.cat, s.tags, s.desc): assert '\t' not in f and '\n' not in f
        return '\t'.join([s.name, s.cat, s.tags, s.desc, ' '.join(toks)])
