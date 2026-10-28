#!/usr/bin/env python3
"""Generates Resources/FactoryPresets.txt for XO: 1500+ presets for trap, rage, hyperpop, plugg and ambient.

Every preset comes from a recipe ("style") with seeded random variation, so the bank is reproducible.
tools/calibrate.py loudness-matches the bank (gains.json); tools/distinct.py finds near-duplicates (reject.json),
which are regenerated with a new seed."""
import json, os, sys
import xo_lib as X
from xo_lib import Patch, U, C, P, R, DIV, ARP, DIST, OMOD, CHORD, lane_gate, lane_steps, lane_pump, lane_sine, lane_ramp
import gen_tools

here = X.here
GAINS = json.load(open(os.path.join(here, 'gains.json'))) if os.path.exists(os.path.join(here, 'gains.json')) else {}
REJECT = set(json.load(open(os.path.join(here, 'reject.json')))) if os.path.exists(os.path.join(here, 'reject.json')) else set()
MX = gen_tools.matrix_bank()
RK = gen_tools.rack_bank()

# ================================================================ finishers
def space(p, amount):
    if amount <= 0.05: return p
    if P(0.7):
        dl, dr = C(('1/8.', '1/8'), ('1/8', '1/8.'), ('1/4', '1/8.'), ('1/8T', '1/8'), ('1/16.', '1/8.'), ('1/4.', '1/4'), ('1/4.', '1/8.'))
        p.fx('dly', l=DIV[dl], r=DIV[dr], fb=U(0.2, 0.55), ping=U(0.3, 1.0), tone=U(0.3, 0.75), mix=round(0.08 + 0.22 * amount * R.uniform(0.6, 1.2), 3))
    p.fx('rev', size=U(0.35, 0.95), decay=U(1.0, 2.5 + 6.0 * amount, 2), damp=U(0.2, 0.7), pre=U(0, 40, 1),
         width=U(0.7, 1.0), mix=round(min(0.55, 0.06 + 0.32 * amount * R.uniform(0.7, 1.3)), 3))
    return p

def chorus(p, mix=None): return p.fx('ch', rate=U(0.2, 1.2), depth=U(0.3, 0.8), mix=mix if mix is not None else U(0.15, 0.45))
def phaser(p): return p.fx('ph', rate=U(0.05, 0.6), depth=U(0.4, 0.9), fb=U(0.2, 0.7), mix=U(0.25, 0.5))
def flanger(p): return p.fx('fl', rate=U(0.05, 0.5), depth=U(0.3, 0.8), fb=U(-0.7, 0.7), mix=U(0.2, 0.4))
def eq(p): return p.fx('eq', low=U(-4, 2, 1), mid=U(-3, 3, 1), midfreq=U(400, 4000, 0), high=U(-2, 5, 1))
def comp(p): return p.fx('comp', thresh=U(-28, -14, 1), ratio=U(2.5, 6, 1), attack=U(2, 20, 1), release=U(60, 220, 0), gain=U(3, 8, 1), mix=U(0.6, 1))
def dist(p, typ=None, drive=None, mix=None):
    return p.fx('dist', type=DIST[typ] if typ else C(0, 1, 2), drive=drive if drive is not None else U(0.2, 0.6), tone=U(0.35, 0.8), mix=mix if mix is not None else U(0.3, 0.8))
def tape(p, a=None):
    a = a if a is not None else U(0.2, 0.8)
    return p.fx('tape', wow=round(a * U(0.3, 0.9), 3), flutter=round(a * U(0.2, 0.7), 3), sat=U(0.15, 0.55), hiss=round(a * U(0.05, 0.35), 3), age=round(a * U(0.3, 0.9), 3), mix=1.0)
def drift(p, lo=0.08, hi=0.4): p.v['drift'] = U(lo, hi); return p
def chord(p, typ, voicing=0, inv=0, strum=0.0): p.v.update(chord_type=CHORD[typ], chord_voicing=voicing, chord_inv=inv, chord_strum=strum, polyphony=32); return p
def vibrato(p, depth=0.004, rate=None): p.lfo(4, 'sine', sync=0, rate=rate or U(4.6, 6.2, 2), mode=1); p.mod('lfo4', 'apitch', depth); return p
def pitch_drop(p, amount=None, time=None, dst='apitch'):
    p.env(3, a=0.5, d=time or U(15, 90, 0), sus=0.0, r=50, curve=0.6)
    p.mod('env3', dst, amount if amount is not None else U(0.06, 0.5)); return p
def mono(p, glide=None, legato=True): p.set(voice_mode=2 if legato else 1, glide=glide if glide is not None else U(30, 160, 0), glide_mode=C(0, 1)); return p
def maybe(p, prob, fn):
    if P(prob): fn(p)
    return p

def macros_std(p):
    p.v['macro1'] = 0.0; p.mod('m1', 'cut1', 0.5)
    p.mod('m2', 'revmix', 0.35); p.mod('m2', 'delaymix', 0.25)
    p.mod('m3', 'distdrive', 0.6); p.mod('m3', 'distmix', 0.5)
    p.mod('m4', 'apos', 0.4); p.mod('m4', 'bpos', 0.3)
    if 'dist_on' not in p.v: p.v.update(dist_on=1, dist_drive=0.0, dist_mix=0.0, dist_type=C(0, 1, 2))
    return p

def flavour_matrix(p, cats):
    pool = [m for m in MX if m.cat in cats]
    m = C(*pool)
    for k, v in m.v.items(): p.v[k] = v
    p.lanes = dict(m.lanes)
    p.v['mx_mix'] = U(0.7, 1.0)
    return m.name

def flavour_rack(p, cats):
    pool = [m for m in RK if m.cat in cats]
    m = C(*pool)
    for k, v in m.v.items(): p.v[k] = v
    p.v['rk_depth'] = U(0.6, 1.0)
    return m.name

# ================================================================ naming
ADJ = ['Rage', 'Vamp', 'Punk', 'Glitter', 'Static', 'Chrome', 'Neon', 'Phantom', 'Velvet', 'Ghost', 'Midnight', 'Cosmic', 'Crystal',
       'Frozen', 'Electric', 'Toxic', 'Liquid', 'Molten', 'Digital', 'Analog', 'Pixel', 'Lunar', 'Solar', 'Stellar', 'Nova', 'Orbit',
       'Eclipse', 'Prism', 'Halo', 'Angel', 'Demon', 'Raven', 'Smoke', 'Fog', 'Haze', 'Storm', 'Thunder', 'Laser', 'Rocket', 'Turbo',
       'Hyper', 'Mega', 'Ultra', 'Super', 'Atomic', 'Plasma', 'Ion', 'Quantum', 'Vapor', 'Retro', 'Future', 'Cyber', 'Glitch',
       'Silver', 'Golden', 'Diamond', 'Onyx', 'Obsidian', 'Ivory', 'Pearl', 'Opal', 'Jade', 'Ruby', 'Cobalt', 'Indigo', 'Violet',
       'Crimson', 'Scarlet', 'Ember', 'Ashen', 'Cinder', 'Frost', 'Ice', 'Snow', 'Winter', 'Summer', 'Autumn', 'Spring', 'Dawn', 'Dusk',
       'Twilight', 'Night', 'Shadow', 'Dark', 'Bright', 'Hollow', 'Empty', 'Lonely', 'Lost', 'Broken', 'Faded', 'Distant', 'Silent',
       'Loud', 'Heavy', 'Wild', 'Savage', 'Feral', 'Wicked', 'Evil', 'Holy', 'Sacred', 'Cursed', 'Blessed', 'Haunted', 'Spirit',
       'Dream', 'Lucid', 'Sleepy', 'Woke', 'Paper', 'Glass', 'Rubber', 'Metal', 'Steel', 'Iron', 'Copper', 'Velour', 'Satin', 'Silk',
       'Cotton', 'Candy', 'Sugar', 'Honey', 'Cherry', 'Berry', 'Lemon', 'Mint', 'Peach', 'Grape', 'Plum', 'Rose', 'Lily', 'Orchid',
       'Lotus', 'Ivy', 'Thorn', 'Moth', 'Butterfly', 'Spider', 'Wolf', 'Tiger', 'Cobra', 'Viper', 'Falcon', 'Hawk', 'Crow', 'Dove',
       'Swan', 'Koi', 'Shark', 'Kraken', 'Titan', 'Giant', 'Tiny', 'Micro', 'Macro', 'Infinite', 'Eternal', 'Endless', 'Final',
       'First', 'Last', 'Zero', 'Alpha', 'Omega', 'Delta', 'Sigma', 'Gamma', 'Vertex', 'Apex', 'Zenith', 'Abyss', 'Void', 'Matrix',
       'Signal', 'Pulse', 'Wave', 'Echo', 'Mirror', 'Shatter', 'Sonic', 'Turbine', 'Engine', 'Motor', 'Velocity', 'Jet', 'Aero',
       'Sky', 'Cloud', 'Rain', 'Ocean', 'Tidal', 'Desert', 'Canyon', 'Arctic', 'Tokyo', 'Paris', 'Neo', 'Retrograde', 'Rewind',
       'Fast', 'Slow', 'Lazy', 'Sharp', 'Smooth', 'Rough', 'Soft', 'Hard', 'Clean', 'Dirty', 'Fuzzy', 'Crunchy', 'Wavy', 'Bouncy']

used = set()
def name_for(nouns):
    for _ in range(3000):
        a = C(*ADJ); n = C(*nouns)
        nm = a + ' ' + n
        if nm not in used and a.lower() not in n.lower():
            used.add(nm); return nm
    raise RuntimeError('names exhausted for %s' % nouns)

# ================================================================ sound building blocks
def env_pluck(p, dec=None):
    dec = dec or U(140, 600, 0)
    p.env(1, a=U(0.5, 3, 1), d=dec, sus=0.0, r=U(dec * 0.5, dec * 1.6, 0), curve=U(0.3, 0.8))
    p.env(2, a=0.5, d=U(dec * 0.25, dec * 0.9, 0), sus=0.0, r=200, curve=U(0.4, 0.8)); return p
def env_lead(p):
    p.env(1, a=U(2, 30, 1), d=U(400, 1500, 0), sus=U(0.6, 0.95), r=U(150, 600, 0), curve=0.3)
    p.env(2, a=U(1, 25, 0), d=U(200, 900, 0), sus=U(0.2, 0.6), r=300); return p
def env_pad(p):
    p.env(1, a=U(350, 2200, 0), d=U(1000, 4000, 0), sus=U(0.6, 0.9), r=U(1200, 4000, 0), curve=0.0)
    p.env(2, a=U(500, 3000, 0), d=U(1500, 5000, 0), sus=U(0.3, 0.7), r=2000, curve=0.0); return p
def env_keys(p):
    p.env(1, a=0.5, d=U(700, 2500, 0), sus=U(0.1, 0.5), r=U(200, 700, 0), curve=0.5)
    p.env(2, a=0.5, d=U(150, 600, 0), sus=0.1, r=300); return p
def env_bell(p):
    p.env(1, a=0.5, d=U(900, 3200, 0), sus=0.0, r=U(700, 2400, 0), curve=0.7)
    p.env(2, a=0.5, d=U(200, 900, 0), sus=0.0, r=400, curve=0.6); return p
def env_stab(p):
    p.env(1, a=0.5, d=U(160, 480, 0), sus=0.0, r=U(100, 300, 0), curve=0.5)
    p.env(2, a=0.5, d=U(80, 300, 0), sus=0.0, r=150, curve=0.6); return p

def fm_pair(p, ratio_semis, amt, mode='fm', carrier='basic', cpos=0.0):
    p.osc(0, carrier, pos=cpos); p.osc(1, 'basic', pos=0.0, semi=ratio_semis, fine=U(-3, 3, 1), level=0.0)
    p.v.update(oa_mod=OMOD[mode], oa_modfrom=1, oa_modamt=amt); return p

# ================================================================ recipes per category
# each recipe: (category, key, count, nouns, tags, description, builder)
RECIPES = []
XOST_NEW = [False]
def recipe(cat, key, count, nouns, tags, desc):
    def deco(fn):
        RECIPES.append(dict(cat=cat, key=key, n=count, nouns=nouns, tags=tags, desc=desc, fn=fn, new=XOST_NEW[0])); return fn
    return deco

# ---------------------------------------------------------------- PLUCK
@recipe('Pluck', 'glass', 22, ['Pluck', 'Shard', 'Glass Pluck', 'Ping'], 'Trap Bouncy Melodic Jetsonmade TM88 Glassy Bright', 'Glassy pluck with a quick filter snap.')
def _(p):
    env_pluck(p); p.osc(0, C('glass', 'bubble', 'sinestack'), pos=U(0.2, 0.9), oct=C(0, 1)); p.osc(1, C('bell', 'glass'), pos=U(0, 0.6), oct=1, semi=C(0, 7, 12), level=U(0.15, 0.4))
    p.flt(1, 'lp12', cut=U(1500, 4500, 0), reso=U(0.1, 0.4), env=U(0.3, 0.6), key=0.4); maybe(p, 0.5, lambda q: pitch_drop(q, U(0.02, 0.12)))
    return space(p, U(0.3, 0.8))

@recipe('Pluck', 'saw', 26, ['Pluck', 'Saw Pluck', 'Pick', 'Snap'], 'Trap Melodic Pierre_Bourne Uzi Richie_Souf Detuned Bright', 'Detuned saw pluck for bouncy trap melodies.')
def _(p):
    env_pluck(p); p.osc(0, C('supersaw', 'basic'), pos=U(0.55, 0.95), uni=C(3, 4, 5, 6), det=U(0.12, 0.38), width=U(0.6, 1.0))
    maybe(p, 0.6, lambda q: q.osc(1, C('basic', 'pwm'), pos=U(0.6, 1.0), oct=C(-1, 1), level=U(0.2, 0.45), uni=C(1, 2), det=0.1))
    p.flt(1, C('lp24', 'ladder', 'lp12'), cut=U(450, 1800, 0), reso=U(0.15, 0.45), env=U(0.4, 0.75), key=0.5)
    return space(p, U(0.2, 0.7))

@recipe('Pluck', 'square', 22, ['Pluck', 'Blip', 'Square Pluck', 'Drop'], 'Trap Plugg Pierre_Bourne Richie_Souf Retro', 'Square pluck with a pitch blip - a plugg and video-game staple.')
def _(p):
    env_pluck(p); p.osc(0, C('basic', 'pwm', 'chip'), pos=C(1.0, 0.3, 0.1, 0.0)); pitch_drop(p, U(0.04, 0.35), U(20, 60, 0))
    p.flt(1, 'lp24', cut=U(800, 3000, 0), reso=U(0.1, 0.35), env=U(0.3, 0.6)); return space(p, U(0.3, 0.8))

@recipe('Pluck', 'bubble', 20, ['Pop', 'Bubble', 'Drop', 'Pluck'], 'Hyperpop Bouncy Starboy Jetsonmade Soft', 'Round bubble pluck with a little pitch pop.')
def _(p):
    env_pluck(p); p.osc(0, 'bubble', pos=U(0.1, 0.8), level=0.85); maybe(p, 0.5, lambda q: q.osc(1, 'basic', pos=0.0, oct=-1, level=U(0.2, 0.4)))
    p.flt(1, 'lp24', cut=U(900, 2600, 0), reso=U(0.2, 0.5), env=U(0.3, 0.55), key=0.5); pitch_drop(p, U(0.08, 0.35), U(20, 60, 0))
    return space(p, U(0.3, 0.8))

@recipe('Pluck', 'muted', 18, ['Mute', 'Tap', 'Pluck', 'Thump'], 'Dark Trap Future TM88 Soft', 'Muted, woody dark pluck.')
def _(p):
    env_pluck(p, U(150, 400, 0)); p.osc(0, C('pluck', 'basic', 'hollow'), pos=U(0.3, 0.8))
    p.flt(1, C('lp24', 'ladder'), cut=U(280, 800, 0), reso=U(0.2, 0.45), env=U(0.35, 0.6), key=0.5); return space(p, U(0.3, 0.9))

@recipe('Pluck', 'harp', 18, ['Harp', 'Lyre', 'Strum'], 'Ambient Melodic Destroy_Lonely Ginseng Clayco Dreamy Chords', 'Harp - chord memory strums a chord from one key.')
def _(p):
    env_pluck(p, U(500, 1100, 0)); p.osc(0, C('pluck', 'sinestack', 'glass'), pos=U(0.2, 0.6)); p.flt(1, 'lp12', cut=U(1500, 4500, 0), reso=0.15, env=0.35, key=0.5)
    chord(p, C('minor', 'min7', 'madd9', 'min9', 'maj7', 'sus2'), voicing=C(0, 1), inv=C(0, 1), strum=U(25, 100, 0)); return space(p, U(0.5, 1.0))

@recipe('Pluck', 'crushed', 16, ['Crush', 'Bits', 'Pluck'], 'Hyperpop Rage Osamason Bleood Gritty LoFi', 'Bitcrushed, dirty pluck.')
def _(p):
    env_pluck(p); p.osc(0, C('digital', 'basic', 'pluck', 'chip'), pos=U(0.3, 1.0), uni=C(1, 2, 3), det=0.18)
    p.flt(1, 'lp12', cut=U(1500, 5000, 0), reso=0.25, env=0.4); p.fx('dist', type=DIST['crush'], drive=U(0.35, 0.8), tone=U(0.3, 0.7), mix=U(0.4, 0.85))
    return space(p, U(0.2, 0.6))

@recipe('Pluck', 'koto', 12, ['Koto', 'Shamisen', 'Pick'], 'Dark Trap Future TM88 Soft', 'Koto-style pluck with a bend-in.')
def _(p):
    env_pluck(p); p.osc(0, C('pluck', 'hollow'), pos=U(0.2, 0.5)); p.flt(1, 'bp', cut=U(800, 2200, 0), reso=0.35, env=0.4)
    p.env(3, a=0.5, d=U(40, 90, 0), sus=0, r=40); p.mod('env3', 'apitch', -0.04); return space(p, U(0.4, 0.9))

@recipe('Pluck', 'nylon', 14, ['Nylon', 'Guitar', 'String'], 'Melodic Trap Uzi Richie_Souf Soft', 'Nylon-guitar-style pluck for sad melodies.')
def _(p):
    env_pluck(p, U(350, 900, 0)); p.osc(0, 'pluck', pos=U(0.1, 0.5)); p.osc(1, 'sinestack', pos=0.2, level=0.3)
    p.flt(1, 'lp24', cut=U(1200, 3200, 0), reso=0.2, env=0.45, key=0.6)
    p.v.update(noise_on=1, noise_type=0, noise_level=0.06, noise_route=0); p.env(3, a=0.5, d=25, sus=0, r=25); p.mod('env3', 'noise', -0.4)
    return space(p, U(0.3, 0.8))

@recipe('Pluck', 'sync', 14, ['Sync Pluck', 'Zap', 'Pluck'], 'Rage Hyperpop Ken_Carson Starboy Bright Retro', 'Squelchy hard-sync pluck.')
def _(p):
    env_pluck(p); p.osc(0, C('basic', 'pwm'), pos=U(0.6, 1.0), warp='sync', warpamt=U(0.25, 0.6), uni=C(1, 2, 3), det=0.15)
    p.mod('env2', 'awarp', U(0.25, 0.5)); p.flt(1, 'lp24', cut=U(1500, 5000, 0), reso=U(0.2, 0.5), env=U(0.2, 0.4)); return space(p, U(0.2, 0.6))

@recipe('Pluck', 'tape', 12, ['Tape Pluck', 'Warp Pluck', 'Cassette'], 'Plugg Ambient Loesoe Ginseng LoFi Dreamy', 'Tape-warped pluck with wobble and hiss.')
def _(p):
    env_pluck(p); p.osc(0, C('pluck', 'basic', 'sinestack'), pos=U(0.2, 0.7)); p.flt(1, 'lp24', cut=U(900, 2500, 0), reso=0.25, env=0.45)
    tape(p, U(0.7, 1.0)); return space(p, U(0.4, 0.9))

@recipe('Pluck', 'fm', 14, ['FM Pluck', 'Tine', 'Pluck'], 'Melodic Trap Jetsonmade Clean Glassy', 'Clean FM pluck, punchy and modern.')
def _(p):
    env_pluck(p); fm_pair(p, C(12, 19, 24, 7), U(0.15, 0.45)); p.mod('env2', 'amod', U(0.2, 0.5)); p.flt(1, 'lp12', cut=U(3000, 9000, 0), reso=0.1)
    return space(p, U(0.2, 0.7))

# ---------------------------------------------------------------- LEAD
@recipe('Lead', 'rage', 30, ['Rage', 'Lead', 'Anthem', 'Riot'], 'Rage Carti WLR Ken_Carson Outtatown Che Gritty Detuned Wide', 'Distorted supersaw rage lead.')
def _(p):
    env_lead(p); p.osc(0, 'supersaw', pos=U(0.7, 1.0), uni=C(7, 8), det=U(0.3, 0.55), width=1.0)
    p.osc(1, 'basic', pos=U(0.66, 1.0), oct=-1, uni=2, det=0.1, level=U(0.3, 0.5))
    p.flt(1, C('ladder', 'lp24'), cut=U(2000, 7000, 0), reso=U(0.1, 0.3), drive=U(0.3, 0.7))
    dist(p, C('overdrive', 'hard', 'tube'), U(0.5, 0.9), U(0.7, 1.0)); maybe(p, 0.5, mono); vibrato(p, 0.003)
    return space(p, U(0.2, 0.6))

@recipe('Lead', 'vamp', 20, ['Vamp', 'Siren', 'Scream'], 'Rage WLR Carti Destroy_Lonely Dark Gritty Haunted', 'Dark vampire-rage lead with an organ edge.')
def _(p):
    env_lead(p); p.osc(0, C('organ', 'hollow', 'supersaw'), pos=U(0.3, 1.0), uni=C(3, 5), det=0.25); p.osc(1, 'supersaw', pos=0.8, oct=1, uni=5, det=0.35, level=0.4)
    p.flt(1, 'ladder', cut=U(1500, 4500, 0), reso=0.25, drive=0.5); dist(p, C('tube', 'overdrive'), U(0.4, 0.8), U(0.5, 0.9))
    chorus(p, 0.3); return space(p, U(0.3, 0.7))

@recipe('Lead', 'siren', 18, ['Siren', 'Alarm', 'Scream', 'Howl'], 'Rage Che Ken_Carson Gritty Mono', 'Screaming hard-sync siren lead.')
def _(p):
    env_lead(p); p.osc(0, 'basic', pos=U(0.66, 1.0), warp='sync', warpamt=U(0.2, 0.5), uni=C(1, 3), det=0.12)
    p.mod('env2', 'awarp', U(0.3, 0.6)); p.flt(1, 'lp24', cut=U(3000, 8000, 0), reso=0.2); dist(p, C('overdrive', 'tube'), U(0.3, 0.6), U(0.4, 0.7))
    mono(p); vibrato(p, 0.004); return space(p, U(0.2, 0.6))

@recipe('Lead', 'glitch', 18, ['Glitch', 'Static', 'Error'], 'Rage Hyperpop Osamason Bleood Ken_Carson Gritty LoFi', 'Crushed glitchy digital lead with stepped motion.')
def _(p):
    env_lead(p); p.osc(0, C('digital', 'chip', 'growl'), pos=U(0, 1), uni=C(2, 4), det=0.3)
    p.lfo(1, C('sh', 'stairs'), div=C('1/16', '1/32', '1/16T')); p.mod('lfo1', 'apos', U(0.3, 0.6))
    p.fx('dist', type=DIST['crush'], drive=U(0.4, 0.8), tone=U(0.4, 0.8), mix=U(0.5, 0.9)); p.flt(1, 'lp12', cut=U(3000, 8000, 0), reso=0.2)
    return space(p, U(0.1, 0.5))

@recipe('Lead', 'plugg', 26, ['Plugg', 'Square', 'Hook', 'Melody'], 'Plugg Trap Pierre_Bourne Richie_Souf Loesoe Mono Dreamy', 'Square plugg lead with glide and vibrato in a big room.')
def _(p):
    env_lead(p); p.osc(0, C('basic', 'pwm', 'chip'), pos=C(1.0, 0.95, 0.1, 0.0), uni=C(1, 1, 2), det=0.08)
    maybe(p, 0.5, lambda q: q.osc(1, 'basic', pos=0.0, oct=-1, level=U(0.2, 0.4)))
    p.flt(1, C('lp12', 'lp24'), cut=U(1800, 5000, 0), reso=U(0.05, 0.25), key=0.4); mono(p); vibrato(p, U(0.002, 0.006))
    maybe(p, 0.4, tape); return space(p, U(0.55, 1.0))

@recipe('Lead', 'sine', 18, ['Sine', 'Whistle', 'Tune'], 'Plugg Ambient Loesoe Ginseng Soft Dreamy', 'Floating sine/whistle lead.')
def _(p):
    env_lead(p); p.osc(0, C('basic', 'sinestack', 'flute'), pos=U(0.0, 0.15), oct=C(0, 1)); p.osc(1, 'basic', pos=0.33, oct=1, level=U(0.06, 0.2))
    p.flt(1, 'lp12', cut=U(2500, 7000, 0), reso=0.1); mono(p); vibrato(p, U(0.003, 0.007)); return space(p, U(0.5, 1.0))

@recipe('Lead', 'uzi', 20, ['Lead', 'Saw', 'Topline', 'Line'], 'Melodic Trap Uzi Pierre_Bourne Starboy Bright Wide', 'Bright, wide melodic lead with movement.')
def _(p):
    env_lead(p); p.osc(0, C('supersaw', 'pwm', 'basic'), pos=U(0.4, 0.95), uni=C(4, 6, 7), det=U(0.15, 0.35), width=1.0)
    p.osc(1, C('glass', 'sinestack'), pos=U(0, 0.6), oct=1, level=U(0.2, 0.35)); p.flt(1, 'lp24', cut=U(3000, 9000, 0), reso=0.15, key=0.3)
    chorus(p, U(0.15, 0.35)); vibrato(p, 0.003); return space(p, U(0.3, 0.7))

@recipe('Lead', 'hyper', 18, ['Hyper Lead', 'Pop Lead', 'Sparkle'], 'Hyperpop Starboy Bleood Bright Gritty', 'Pitched-up hyperpop lead with trills.')
def _(p):
    env_lead(p); p.osc(0, C('digital', 'supersaw', 'chip'), pos=U(0.3, 1.0), oct=1, uni=C(5, 7), det=U(0.25, 0.4)); p.osc(1, 'pwm', pos=U(0.3, 1.0), oct=2, level=U(0.15, 0.3))
    p.flt(1, 'lp12', cut=U(4000, 10000, 0), reso=0.2); p.fx('dist', type=DIST['crush'], drive=U(0.25, 0.6), tone=0.8, mix=U(0.3, 0.6))
    p.lfo(1, C('square', 'stairs'), div=C('1/16', '1/8')); p.mod('lfo1', 'apitch', C(0.5, 0.25, 0.29)); return space(p, U(0.2, 0.5))

@recipe('Lead', 'vocal', 14, ['Voice', 'Vox Lead', 'Choir Lead'], 'Hyperpop Melodic Starboy Destroy_Lonely Dreamy', 'Formant vocal lead that sings with the mod wheel.')
def _(p):
    env_lead(p); p.osc(0, 'vocal', pos=U(0.1, 0.9), uni=C(1, 2, 3, 4), det=0.2); p.flt(1, 'formant', cut=U(300, 1500, 0), reso=U(0.4, 0.7))
    p.mod('wheel', 'cut1', 0.5); p.mod('wheel', 'apos', 0.4); vibrato(p, 0.004); return space(p, U(0.4, 0.8))

@recipe('Lead', 'future', 16, ['Lead', 'Brass Lead', 'Horn'], 'Dark Trap Future TM88 Gritty', 'Dark trap synth-brass lead.')
def _(p):
    p.env(1, a=U(15, 60, 0), d=900, sus=0.8, r=300); p.env(2, a=U(30, 120, 0), d=500, sus=0.5, r=300)
    p.osc(0, C('basic', 'supersaw'), pos=0.66, uni=C(2, 3), det=0.12); p.osc(1, 'basic', pos=0.66, oct=-1, level=0.4)
    p.flt(1, 'lp24', cut=U(600, 1500, 0), reso=0.2, env=U(0.3, 0.5)); dist(p, 'tube', U(0.15, 0.35), 0.4); return space(p, U(0.4, 0.8))

@recipe('Lead', 'flute', 14, ['Flute', 'Pipe', 'Breath'], 'Dark Trap Future TM88 Plugg Soft', 'Breathy flute lead - a dark trap classic.')
def _(p):
    env_lead(p); p.osc(0, 'flute', pos=U(0, 0.6)); p.v.update(noise_on=1, noise_type=1, noise_level=U(0.06, 0.15), noise_route=0)
    p.flt(1, 'lp12', cut=U(2500, 6500, 0), reso=0.15, key=0.5); vibrato(p, U(0.003, 0.006)); maybe(p, 0.6, mono); return space(p, U(0.5, 0.95))

@recipe('Lead', 'growl', 12, ['Growl', 'Beast', 'Howl'], 'Rage Dark Che Bleood Gritty', 'Growling FM lead, nasty and low.')
def _(p):
    env_lead(p); p.osc(0, 'growl', pos=U(0, 1)); p.osc(1, 'basic', pos=0.0, oct=-1, level=0.4)
    p.lfo(1, 'sine', div=C('1/8', '1/4', '1/8T')); p.mod('lfo1', 'apos', U(0.3, 0.5)); p.flt(1, 'lp24', cut=U(1200, 3000, 0), reso=0.3)
    maybe(p, 0.5, lambda q: dist(q, 'hard', U(0.3, 0.6), 0.6)); return space(p, U(0.2, 0.5))

# ---------------------------------------------------------------- BELL
@recipe('Bell', 'fm', 26, ['Bell', 'Bells', 'Chime'], 'Trap Dark Future TM88 Jetsonmade Glassy', 'FM bell with a long shimmering tail.')
def _(p):
    env_bell(p); fm_pair(p, C(12, 19, 24, 31, 17), U(0.2, 0.5)); p.mod('env2', 'amod', U(0.2, 0.5))
    p.flt(1, 'lp12', cut=U(2500, 9000, 0), reso=0.1, key=0.3); return space(p, U(0.4, 0.9))

@recipe('Bell', 'musicbox', 24, ['Music Box', 'Lullaby', 'Toybox'], 'Dark Plugg Loesoe Ginseng TM88 Haunted LoFi', 'Detuned, wobbly music box.')
def _(p):
    p.env(1, a=0.3, d=U(500, 1300, 0), sus=0, r=U(400, 1100, 0), curve=0.7)
    p.osc(0, 'sinestack', pos=U(0, 0.3), oct=C(1, 2)); p.osc(1, 'basic', pos=0.0, oct=3, level=U(0.1, 0.25), fine=U(-15, 15, 1))
    p.flt(1, 'lp12', cut=U(4000, 8000, 0), reso=0.1); maybe(p, 0.7, lambda q: tape(q, U(0.5, 1.0))); maybe(p, 0.6, lambda q: drift(q, 0.2, 0.6))
    return space(p, U(0.6, 1.0))

@recipe('Bell', 'glass', 18, ['Crystal', 'Glass Bell', 'Chime'], 'Ambient Melodic Clayco Destroy_Lonely Glassy Dreamy', 'Crystal bell built from glass partials.')
def _(p):
    env_bell(p); p.osc(0, 'glass', pos=U(0.4, 1.0), oct=1); p.osc(1, 'bell', pos=U(0, 1), oct=1, level=0.3); p.flt(1, 'lp12', cut=U(4000, 10000, 0), reso=0.1)
    return space(p, U(0.5, 1.0))

@recipe('Bell', 'glock', 16, ['Glock', 'Glockenspiel', 'Mallet'], 'Trap Bouncy Jetsonmade Pierre_Bourne Bright', 'Glockenspiel for bouncy melodies.')
def _(p):
    env_bell(p); p.osc(0, 'sinestack', pos=U(0.2, 0.5), oct=1); p.osc(1, 'basic', pos=0.0, oct=2, semi=C(7, 0), level=0.2)
    p.mod('env3', 'blevel', -0.6); p.env(3, a=0.5, d=U(60, 150, 0), sus=0, r=60); p.flt(1, 'lp12', cut=U(5000, 10000, 0)); return space(p, U(0.3, 0.8))

@recipe('Bell', 'ring', 12, ['Ring Bell', 'Gong', 'Clang'], 'Dark Rage Osamason Gritty Haunted', 'Ring-mod bell, metallic and wrong.')
def _(p):
    env_bell(p); p.osc(0, C('bell', 'glass'), pos=U(0, 1)); p.osc(1, 'basic', pos=0.0, semi=C(6, 11, 13, 18), level=0.0)
    p.v.update(oa_mod=OMOD['ring'], oa_modfrom=1, oa_modamt=U(0.3, 0.7)); p.flt(1, 'lp12', cut=U(3000, 8000, 0)); return space(p, U(0.4, 0.9))

@recipe('Bell', 'celesta', 14, ['Celesta', 'Twinkle', 'Star Bell'], 'Melodic Trap Uzi Richie_Souf Soft Dreamy', 'Celesta for sweet or eerie melodies.')
def _(p):
    env_bell(p); fm_pair(p, 24, U(0.1, 0.25), 'pm'); p.v['oa_oct'] = 1; p.mod('env2', 'amod', 0.3); p.flt(1, 'lp12', cut=U(4000, 9000, 0))
    return space(p, U(0.4, 0.9))

@recipe('Bell', 'toy', 12, ['Toy Bell', 'Toy', 'Charm'], 'Plugg Hyperpop Loesoe Starboy LoFi Retro', 'Broken toy bell, a little out of tune.')
def _(p):
    p.env(1, a=0.3, d=U(300, 800, 0), sus=0, r=U(300, 700, 0)); p.osc(0, C('chip', 'basic'), pos=C(0.0, 0.33, 0.5), oct=1)
    p.osc(1, 'sinestack', pos=0.1, oct=2, level=0.3, fine=U(-25, 25, 1)); p.flt(1, 'lp12', cut=U(3000, 7000, 0)); maybe(p, 0.6, lambda q: tape(q, U(0.5, 1.0)))
    return space(p, U(0.4, 0.9))

@recipe('Bell', 'haunted', 16, ['Bells', 'Bell Tower', 'Carillon'], 'Dark Ambient Loesoe Ginseng TM88 Haunted Wide', 'Haunted bells in a huge cold room.')
def _(p):
    env_bell(p); p.osc(0, C('bell', 'glass'), pos=U(0, 1), uni=C(2, 3), det=0.12); chorus(p, U(0.3, 0.5))
    p.env(1, a=0.5, d=U(1500, 4000, 0), sus=0, r=U(2000, 4000, 0)); p.flt(1, 'lp12', cut=U(2500, 7000, 0)); return space(p, U(0.8, 1.0))

# ---------------------------------------------------------------- KEYS
@recipe('Keys', 'ep', 26, ['EP', 'Keys', 'Electric Piano'], 'Melodic Trap Richie_Souf Uzi Future Soft Dreamy', 'Soft electric piano.')
def _(p):
    env_keys(p); fm_pair(p, C(12, 24), U(0.1, 0.3), 'pm'); p.mod('vel', 'amod', 0.3); p.flt(1, 'lp12', cut=U(2500, 5500, 0), reso=0.1)
    chorus(p, U(0.15, 0.4)); maybe(p, 0.45, lambda q: chord(q, C('min9', 'min7', 'madd9', 'maj7', 'maj9'), voicing=C(0, 1), inv=C(0, 1, 2), strum=U(0, 45, 0)))
    p.set(polyphony=24); return space(p, U(0.3, 0.8))

@recipe('Keys', 'piano', 22, ['Piano', 'Upright', 'Grand'], 'Dark Trap Future TM88 Richie_Souf LoFi Soft', 'Dark felt piano, a trap staple.')
def _(p):
    env_keys(p); p.osc(0, 'sinestack', pos=U(0.2, 0.6)); p.osc(1, 'pluck', pos=0.3, oct=1, level=0.3)
    p.flt(1, 'lp12', cut=U(1500, 4000, 0), reso=0.1, env=0.3, key=0.5); p.v.update(noise_on=1, noise_type=C(0, 3), noise_level=U(0.03, 0.1), noise_route=3)
    maybe(p, 0.5, lambda q: tape(q, U(0.3, 0.8))); p.set(polyphony=24); return space(p, U(0.3, 0.8))

@recipe('Keys', 'organ', 16, ['Organ', 'Chapel', 'Hymn'], 'Dark Rage WLR Carti Haunted', 'Haunted church organ.')
def _(p):
    p.osc(0, 'organ', pos=U(0, 1)); p.env(1, a=3, d=500, sus=1.0, r=180); p.fx('ch', rate=U(5, 7, 2), depth=0.4, mix=0.5); p.flt(1, 'lp12', cut=U(2500, 6500, 0))
    maybe(p, 0.4, lambda q: chord(q, C('minor', 'min7', 'phryg'), voicing=C(0, 2))); p.set(polyphony=24); return space(p, U(0.4, 0.9))

@recipe('Keys', 'rhodes', 16, ['Rhodes', 'Tines', 'Tremolo EP'], 'Melodic Ambient Ginseng Clayco Soft Dreamy', 'Warm Rhodes with tremolo.')
def _(p):
    env_keys(p); fm_pair(p, 12, U(0.08, 0.18)); p.mod('vel', 'amod', 0.25); p.lfo(2, 'sine', div=C('1/8', '1/8T', '1/16'), mode=0); p.mod('lfo2', 'amp', -0.35)
    p.flt(1, 'lp12', cut=3500, reso=0.1); p.set(polyphony=24); return space(p, U(0.3, 0.8))

@recipe('Keys', 'harpsi', 14, ['Harpsichord', 'Clavecin'], 'Dark Rage Carti WLR Haunted Retro', 'Evil harpsichord.')
def _(p):
    p.osc(0, C('pwm', 'pluck'), pos=U(0.1, 0.4)); p.osc(1, 'pwm', pos=0.6, oct=1, level=0.35)
    p.env(1, a=0.3, d=U(600, 1400, 0), sus=0.0, r=200); p.flt(1, 'lp12', cut=U(3000, 7000, 0), reso=0.15, env=0.2); p.set(polyphony=24)
    return space(p, U(0.3, 0.8))

@recipe('Keys', 'clav', 10, ['Clav', 'Clavinet'], 'Bouncy Trap Pierre_Bourne Retro', 'Funky clav keys.')
def _(p):
    p.osc(0, C('pwm', 'pluck'), pos=U(0.2, 0.8)); p.flt(1, 'bp', cut=U(700, 1800, 0), reso=0.5, env=0.4); p.env(1, a=0.3, d=U(250, 600, 0), sus=0.1, r=80)
    return space(p, U(0.2, 0.6))

@recipe('Keys', 'chordkeys', 18, ['Chords', 'Keys', 'Progression'], 'Melodic Plugg Richie_Souf Destroy_Lonely Chords Dreamy', 'One key plays a full chord with a slow roll.')
def _(p):
    env_keys(p); p.osc(0, C('sinestack', 'organ', 'hollow', 'basic'), pos=U(0.1, 0.6)); p.flt(1, 'lp12', cut=U(1800, 4500, 0), reso=0.1)
    chord(p, C('min9', 'min7', 'maj9', 'madd9', 'sus2'), voicing=C(1, 2), inv=C(0, 1), strum=U(15, 80, 0)); chorus(p, 0.3); return space(p, U(0.4, 0.9))

# ---------------------------------------------------------------- PAD
@recipe('Pad', 'dream', 24, ['Pad', 'Dream', 'Haze', 'Bloom'], 'Ambient Melodic Destroy_Lonely Ginseng Clayco Dreamy Wide', 'Dreamy wavetable pad, slowly morphing.')
def _(p):
    env_pad(p); p.lfo(1, C('sine', 'tri', 'smooth'), div=C('2bar', '1bar', '4bar'), mode=0)
    p.osc(0, C('sweep', 'bubble', 'sinestack', 'hollow'), pos=U(0.1, 0.5), uni=C(3, 4), det=U(0.1, 0.25)); p.osc(1, C('glass', 'flute'), pos=U(0, 0.6), oct=1, level=0.35, uni=2)
    p.mod('lfo1', 'apos', U(0.2, 0.4)); p.flt(1, 'lp24', cut=U(1200, 3500, 0), reso=0.15, env=0.15); return space(p, U(0.6, 1.0))

@recipe('Pad', 'saw', 20, ['Strings', 'Saw Pad', 'Wall'], 'Rage Trap Outtatown Ken_Carson Wide Detuned', 'Huge detuned saw pad.')
def _(p):
    env_pad(p); p.osc(0, 'supersaw', pos=U(0.5, 0.9), uni=8, det=U(0.25, 0.45), width=1.0); p.osc(1, 'supersaw', pos=U(0.4, 0.8), oct=-1, uni=6, det=0.3, level=0.45)
    p.flt(1, 'lp24', cut=U(1000, 3500, 0), reso=0.15, env=0.15); chorus(p, U(0.25, 0.45)); return space(p, U(0.5, 0.9))

@recipe('Pad', 'choir', 22, ['Choir', 'Hymn', 'Requiem', 'Chant'], 'Dark Ambient WLR TM88 Loesoe Haunted', 'Dark choir pad, ghosts humming.')
def _(p):
    env_pad(p); p.lfo(1, 'smooth', sync=0, rate=U(0.1, 0.5, 2), mode=0)
    p.osc(0, 'vocal', pos=U(0.1, 0.9), uni=C(4, 6), det=0.2); p.flt(1, 'formant', cut=U(300, 1500, 0), reso=0.5); p.mod('lfo1', 'cut1', 0.15)
    chorus(p); return space(p, U(0.6, 1.0))

@recipe('Pad', 'glass', 16, ['Glass Pad', 'Frost Pad', 'Crystal'], 'Ambient Clayco Ginseng Glassy Wide', 'Frozen glass pad.')
def _(p):
    env_pad(p); p.lfo(1, 'sine', div='2bar', mode=0); p.osc(0, 'glass', pos=U(0.3, 1.0), uni=C(2, 4), det=0.12, oct=1); p.osc(1, 'sinestack', pos=0.6, level=0.4)
    p.mod('lfo1', 'apos', 0.3); p.flt(1, 'lp12', cut=U(3000, 8000, 0), reso=0.1); p.flt(2, 'hp12', cut=U(150, 400, 0), reso=0.1); return space(p, U(0.6, 1.0))

@recipe('Pad', 'dark', 18, ['Drone', 'Shadow Pad', 'Abyss'], 'Dark Trap Future TM88 Haunted', 'Dark, filtered pad with crackle.')
def _(p):
    env_pad(p); p.osc(0, C('hollow', 'growl', 'sweep'), pos=U(0.1, 0.5), uni=C(2, 4), det=0.2); p.osc(1, 'basic', pos=0.0, oct=-1, level=0.4)
    p.flt(1, C('ladder', 'lp24'), cut=U(350, 1000, 0), reso=0.3, env=0.25, drive=0.3); p.v.update(noise_on=1, noise_type=C(2, 3), noise_level=U(0.1, 0.25), noise_route=3)
    return space(p, U(0.6, 1.0))

@recipe('Pad', 'fog', 14, ['Fog', 'Mist', 'Air'], 'Ambient Loesoe Ginseng Soft Dreamy', 'Breathy noise-and-sine pad.')
def _(p):
    env_pad(p); p.lfo(1, 'smooth', sync=0, rate=0.2, mode=0)
    p.osc(0, C('sinestack', 'flute', 'basic'), pos=U(0, 0.4), uni=2, det=0.1); p.v.update(noise_on=1, noise_type=1, noise_level=U(0.25, 0.45), noise_route=1, noise_color=U(0.3, 0.7))
    p.flt(2, 'bp', cut=U(800, 3000, 0), reso=0.4); p.mod('lfo1', 'cut2', 0.3); p.flt(1, 'lp12', cut=5000, reso=0.1); return space(p, U(0.7, 1.0))

@recipe('Pad', 'evolve', 18, ['Atmosphere', 'Spirits', 'Apparition'], 'Ambient Dark Clayco Loesoe Haunted Moving', 'Evolving pad, never the same twice.')
def _(p):
    env_pad(p); p.osc(0, C('sweep', 'vocal', 'bell', 'growl'), pos=U(0, 1), uni=C(3, 4), det=0.2); p.osc(1, C('glass', 'hollow'), pos=U(0, 1), oct=C(0, 1), level=0.4, uni=2)
    p.lfo(3, 'smooth', sync=0, rate=U(0.05, 0.3, 3), mode=0); p.lfo(1, 'sine', div='2bar', mode=0)
    p.mod('lfo3', 'apos', 0.4); p.mod('lfo1', 'bpos', -0.4); p.mod('lfo3', 'cut1', 0.2); p.flt(1, 'lp24', cut=U(1200, 3500, 0), reso=0.25)
    return space(p, U(0.7, 1.0))

@recipe('Pad', 'chordpad', 14, ['Chord Pad', 'Chords', 'Horizon'], 'Melodic Ambient Destroy_Lonely Clayco Chords Wide', 'One-finger chord pad.')
def _(p):
    env_pad(p); p.osc(0, C('supersaw', 'hollow', 'sweep', 'vocal'), pos=U(0.2, 0.8), uni=C(3, 4), det=0.18); p.flt(1, 'lp24', cut=U(900, 2500, 0), reso=0.2)
    chord(p, C('min9', 'min7', 'madd9', 'sus2', 'maj9'), voicing=C(1, 2), inv=C(0, 1)); return space(p, U(0.6, 1.0))

@recipe('Pad', 'hyper', 12, ['Hyper Pad', 'Neon Pad', 'Sky'], 'Hyperpop Starboy Bright Wide', 'Bright phasey hyperpop pad.')
def _(p):
    env_pad(p); p.osc(0, C('digital', 'pwm', 'supersaw'), pos=U(0.3, 1.0), uni=C(5, 8), det=0.35, oct=1); p.flt(1, 'lp12', cut=8000, reso=0.2); phaser(p)
    return space(p, U(0.5, 0.9))

# ---------------------------------------------------------------- ARP
def arp_base(p):
    p.set(arp_on=1, arp_gate=U(0.3, 0.75), arp_oct=C(1, 2, 2, 3), arp_swing=C(0.0, 0.0, U(0.05, 0.25)),
          arp_rate=DIV[C('1/16', '1/16', '1/8', '1/16T', '1/8T')], arp_mode=ARP[C('up', 'updown', 'down', 'played')])
    dec = U(120, 420, 0)
    p.env(1, a=0.5, d=dec, sus=U(0.0, 0.2), r=U(80, 280, 0), curve=0.5); p.env(2, a=0.5, d=U(80, 250, 0), sus=0, r=150, curve=0.6)
    p.mod('vel', 'cut1', 0.2); return p

@recipe('Arp', 'glass', 18, ['Arp', 'Cascade', 'Spiral'], 'Melodic Trap Jetsonmade Uzi Glassy Arp', 'Glassy arpeggio through two octaves.')
def _(p):
    arp_base(p); p.osc(0, C('glass', 'bubble', 'bell'), pos=U(0.1, 0.8), oct=1); p.flt(1, 'lp12', cut=U(1500, 4500, 0), reso=0.3, env=U(0.3, 0.5)); return space(p, U(0.4, 0.8))

@recipe('Arp', 'seq', 26, ['Sequence', 'Steps', 'Pattern'], 'Trap Bouncy Pierre_Bourne Jetsonmade Arp', 'Hold a chord: the step grid plays a melody from it.')
def _(p):
    arp_base(p); p.osc(0, C('pluck', 'basic', 'bubble', 'glass', 'hollow'), pos=U(0.2, 0.7), uni=C(1, 2, 3), det=0.15)
    p.flt(1, 'lp24', cut=U(700, 2500, 0), reso=U(0.2, 0.4), env=U(0.3, 0.6))
    p.set(arp_mode=ARP['seq'], arp_steps=C(8, 16, 16, 12), arp_rate=DIV[C('1/16', '1/16', '1/8', '1/16T')], arp_oct=1)
    scale = C([0, 3, 5, 7, 10, 12], [0, 2, 3, 7, 10, 12], [0, 4, 7, 9, 12, 14], [0, 3, 7, 10, 12, -2], [0, 1, 7, 8, 12, 3], [0, 3, 6, 9, 12, -3])
    for s in range(16):
        p.v['st%d_on' % (s + 1)] = 1 if P(0.72) or s % 4 == 0 else 0
        p.v['st%d_tr' % (s + 1)] = C(*scale) if s % 4 else C(0, 0, 12, -12)
        p.v['st%d_vel' % (s + 1)] = 1.0 if s % 4 == 0 else U(0.4, 0.9)
    return space(p, U(0.3, 0.8))

@recipe('Arp', 'trance', 14, ['Gate Arp', 'Run', 'Rush'], 'Rage Outtatown Ken_Carson Detuned Wide Arp', 'Supersaw arp for rage drops.')
def _(p):
    arp_base(p); p.osc(0, 'supersaw', pos=U(0.6, 1.0), uni=C(6, 8), det=U(0.25, 0.45), width=1.0); p.flt(1, 'lp24', cut=U(800, 2200, 0), reso=U(0.2, 0.4), env=U(0.35, 0.6))
    maybe(p, 0.5, lambda q: dist(q, 'overdrive', U(0.3, 0.6), 0.6)); return space(p, U(0.3, 0.7))

@recipe('Arp', 'chip', 14, ['Chip Arp', 'Arcade', 'Cartridge'], 'Hyperpop Plugg Pierre_Bourne Starboy Retro Arp', 'Video-game arpeggio.')
def _(p):
    arp_base(p); p.osc(0, 'chip', pos=U(0, 1)); pitch_drop(p, U(0.2, 0.5), 25); p.flt(1, 'lp12', cut=8000, reso=0.1)
    p.set(arp_rate=DIV[C('1/16', '1/32', '1/16T')], arp_oct=C(1, 2)); return space(p, U(0.2, 0.6))

@recipe('Arp', 'dark', 14, ['Triplets', 'Gallop', 'Maze'], 'Dark Trap Future TM88 Gritty Arp', 'Dark triplet arp with grit.')
def _(p):
    arp_base(p); p.osc(0, C('hollow', 'growl', 'vocal'), pos=U(0.1, 0.6), uni=C(1, 2)); p.osc(1, 'sinestack', oct=-1, level=0.35)
    p.flt(1, 'ladder', cut=U(400, 1100, 0), reso=U(0.3, 0.6), env=U(0.3, 0.5), drive=U(0.3, 0.6)); p.set(arp_rate=DIV[C('1/8T', '1/16T')], arp_mode=ARP[C('down', 'updown')])
    dist(p, 'tube', U(0.2, 0.5), 0.5); return space(p, U(0.4, 0.8))

@recipe('Arp', 'random', 12, ['Sparkle', 'Scatter', 'Dice'], 'Ambient Hyperpop Clayco Starboy Dreamy Arp', 'Random sparkle arp.')
def _(p):
    arp_base(p); p.osc(0, C('glass', 'sinestack', 'bell', 'bubble'), pos=U(0, 1), oct=1); p.set(arp_mode=ARP['random'], arp_oct=C(2, 3))
    p.lfo(1, 'sh', div='1/16'); p.mod('lfo1', 'apos', U(0.2, 0.5)); p.flt(1, 'lp12', cut=U(3000, 8000, 0), reso=0.15); return space(p, U(0.5, 0.9))

@recipe('Arp', 'chordarp', 14, ['Chord Arp', 'Cascade', 'Ladder'], 'Melodic Plugg Richie_Souf Destroy_Lonely Chords Arp', 'One key: chord memory builds a chord and the arp runs it.')
def _(p):
    arp_base(p); p.osc(0, C('pluck', 'glass', 'sinestack', 'bell'), pos=U(0.2, 0.7)); p.flt(1, 'lp12', cut=U(1500, 4500, 0), reso=0.2, env=0.4)
    chord(p, C('min7', 'min9', 'madd9', 'minor', 'maj7'), voicing=C(0, 1)); p.set(arp_mode=ARP[C('up', 'updown', 'played')], arp_oct=C(1, 2)); return space(p, U(0.4, 0.8))

@recipe('Arp', 'musicbox', 10, ['Music Box Arp', 'Lullaby Arp'], 'Plugg Dark Loesoe Haunted LoFi Arp', 'Wobbly music-box arpeggio.')
def _(p):
    arp_base(p); p.osc(0, 'sinestack', pos=U(0, 0.3), oct=1); p.osc(1, 'basic', pos=0.0, oct=2, level=0.2, fine=U(-15, 15, 1))
    tape(p, U(0.6, 1.0)); p.flt(1, 'lp12', cut=6000); return space(p, U(0.5, 0.9))

@recipe('Arp', 'pluggarp', 14, ['Arp', 'Run', 'Skip'], 'Plugg Bouncy Pierre_Bourne Richie_Souf Soft Arp', 'Swung plugg pluck arp.')
def _(p):
    arp_base(p); p.osc(0, C('pluck', 'basic', 'chip'), pos=U(0.2, 1.0)); p.flt(1, 'lp24', cut=U(700, 2200, 0), reso=0.3, env=U(0.4, 0.6))
    p.set(arp_swing=U(0.15, 0.35)); return space(p, U(0.4, 0.9))

# ---------------------------------------------------------------- BASS
def bass_base(p):
    p.set(voice_mode=2, glide=U(40, 140, 0), glide_mode=1, polyphony=1)
    p.env(1, a=1, d=U(1200, 3500, 0), sus=0.0, r=U(150, 400, 0), curve=0.0); return p

@recipe('Bass', '808', 26, ['808', 'Boom', 'Low End'], 'Trap Future TM88 Dark Mono', 'Long 808 with a pitch punch - play legato for slides.')
def _(p):
    bass_base(p); p.osc(0, 'basic', pos=C(0.0, 0.0, 0.1), oct=-1); pitch_drop(p, U(0.1, 0.3), U(25, 50, 0))
    p.v.update(shp_on=1, shp_type=C(0, 2), shp_drive=U(0.2, 0.55), shp_pos=1); p.flt(1, 'lp24', cut=U(800, 2500, 0), reso=0.05)
    p.set(sub_on=1, sub_shape=0, sub_oct=2, sub_level=U(0.3, 0.6), sub_route=3); return p

@recipe('Bass', 'dirty', 24, ['808', 'Dirty 808', 'Slam', 'Distorted 808'], 'Rage Trap Carti Ken_Carson Che Gritty Mono', 'Distorted 808 that cuts through phone speakers.')
def _(p):
    bass_base(p); p.osc(0, 'basic', pos=C(0.0, 0.1), oct=-1); pitch_drop(p, U(0.15, 0.35), U(25, 60, 0))
    p.v.update(shp_on=1, shp_type=C(1, 2, 3), shp_drive=U(0.5, 0.95), shp_pos=1); dist(p, C('overdrive', 'tube', 'hard'), U(0.4, 0.85), U(0.5, 0.95))
    p.flt(1, 'lp24', cut=U(1500, 5000, 0), reso=0.1); p.set(sub_on=1, sub_shape=0, sub_oct=2, sub_level=U(0.3, 0.6), sub_route=3); return p

@recipe('Bass', 'glide', 16, ['Slide', 'Glide 808'], 'Trap Plugg Future Richie_Souf Mono', '808 with a long portamento, made for slides.')
def _(p):
    bass_base(p); p.osc(0, C('basic', 'basic', 'pwm', 'hollow'), pos=U(0.0, 0.5), oct=-1); p.set(glide=U(120, 380, 0), glide_mode=C(0, 1))
    p.v.update(shp_on=1, shp_type=C(0, 1, 2, 3), shp_drive=U(0.15, 0.85), shp_pos=C(0, 1)); p.flt(1, C('lp24', 'ladder'), cut=U(500, 3000, 0), reso=U(0.0, 0.35), drive=U(0, 0.5))
    maybe(p, 0.5, lambda q: dist(q, C('tube', 'overdrive', 'fold'), U(0.2, 0.6), U(0.3, 0.8)))
    p.set(sub_on=1, sub_shape=0, sub_oct=2, sub_level=U(0.2, 0.6), sub_route=3); return p

@recipe('Bass', 'reese', 14, ['Reese', 'Growl Bass'], 'Dark Rage Bleood Gritty Detuned', 'Dark detuned reese.')
def _(p):
    bass_base(p); p.osc(0, 'basic', pos=0.66, oct=-1, uni=C(2, 3), det=0.3); p.osc(1, 'basic', pos=0.66, oct=-1, fine=U(10, 25, 1), level=0.6)
    p.flt(1, 'lp24', cut=U(300, 900, 0), reso=0.3); p.lfo(1, 'sine', div='2bar', mode=0); p.mod('lfo1', 'cut1', 0.3); p.env(1, a=1, d=2000, sus=0.7, r=300); return p

@recipe('Bass', 'sub', 10, ['Sub', 'Low End'], 'Trap Ambient Clean Mono', 'Clean, round sub - with a touch of warmth.')
def _(p):
    bass_base(p); p.osc(0, 'basic', pos=U(0.0, 0.4), oct=-1); p.flt(1, C('lp12', 'lp24'), cut=U(250, 1500, 0), reso=U(0.0, 0.3), env=U(0.0, 0.4))
    p.env(1, a=U(1, 20, 0), d=U(600, 3000, 0), sus=U(0.0, 0.8), r=U(80, 500, 0), curve=U(-0.3, 0.6))
    p.v.update(shp_on=1, shp_type=C(0, 1, 2, 3), shp_drive=U(0.05, 0.7)); maybe(p, 0.5, lambda q: pitch_drop(q, U(0.05, 0.4), U(20, 120, 0)))
    maybe(p, 0.4, lambda q: q.set(sub_on=1, sub_shape=C(0, 1), sub_oct=C(1, 2), sub_level=U(0.2, 0.5), sub_route=3)); return p

@recipe('Bass', 'pluckbass', 16, ['Bass', 'Pluck Bass', 'Thump'], 'Bouncy Plugg Pierre_Bourne Jetsonmade Mono', 'Short bouncy pluck bass.')
def _(p):
    bass_base(p); p.osc(0, C('basic', 'pwm'), pos=U(0.6, 1.0), oct=-1); p.env(1, a=0.5, d=U(200, 450, 0), sus=0.0, r=100)
    p.flt(1, 'ladder', cut=U(300, 800, 0), reso=U(0.3, 0.6), env=U(0.4, 0.6)); p.set(voice_mode=1); return p

@recipe('Bass', 'wobble', 10, ['Wobble', 'Wub'], 'Rage Hyperpop Bleood Gritty Moving', 'Tempo-synced wobble bass.')
def _(p):
    bass_base(p); p.osc(0, C('growl', 'supersaw'), pos=U(0.2, 0.8), oct=-1, uni=C(1, 2)); p.lfo(1, 'sine', div=C('1/8', '1/8T', '1/4'), mode=1)
    p.flt(1, 'lp24', cut=U(300, 700, 0), reso=0.4); p.mod('lfo1', 'cut1', U(0.4, 0.6)); p.env(1, a=1, d=2000, sus=0.7, r=200); return p

# ---------------------------------------------------------------- STAB / SYNTH
@recipe('Stab', 'chord', 22, ['Stab', 'Chord Stab', 'Hit'], 'Rage Trap Ken_Carson Outtatown Chords Gritty', 'One key = a full minor chord stab.')
def _(p):
    env_stab(p); p.osc(0, C('supersaw', 'basic', 'pwm', 'organ'), pos=U(0.4, 1.0), uni=C(3, 5, 7), det=0.25); p.flt(1, 'lp24', cut=U(500, 1600, 0), reso=0.3, env=0.5)
    chord(p, C('min7', 'minor', 'min9', 'dim', 'power')); maybe(p, 0.5, lambda q: dist(q, C('overdrive', 'hard'), U(0.3, 0.7), 0.7)); return space(p, U(0.2, 0.6))

@recipe('Stab', 'brass', 16, ['Brass', 'Horn Stab', 'Fanfare'], 'Dark Trap Future TM88 Gritty', 'Dark synth-brass stab.')
def _(p):
    p.env(1, a=U(8, 25, 0), d=U(300, 700, 0), sus=0.2, r=200); p.env(2, a=U(15, 40, 0), d=300, sus=0.2, r=200)
    p.osc(0, 'basic', pos=0.66, uni=3, det=0.15); p.osc(1, 'basic', pos=0.66, oct=-1, level=0.4); p.flt(1, 'lp24', cut=U(500, 1200, 0), reso=0.2, env=0.55)
    maybe(p, 0.5, lambda q: chord(q, C('minor', 'power', 'oct'))); return space(p, U(0.3, 0.7))

@recipe('Stab', 'rave', 14, ['Rave Stab', 'Hoover', 'Siren Stab'], 'Rage Hyperpop Ken_Carson Bleood Gritty Detuned', 'Rave hoover-style stab.')
def _(p):
    env_stab(p); p.osc(0, 'supersaw', pos=1.0, uni=8, det=0.5, width=1.0); p.osc(1, 'pwm', pos=0.2, oct=-1, uni=3, det=0.3, level=0.5)
    pitch_drop(p, U(-0.15, -0.05), U(80, 200, 0)); p.flt(1, 'lp24', cut=U(2000, 6000, 0), reso=0.15); dist(p, 'overdrive', U(0.3, 0.6), 0.6); return space(p, U(0.2, 0.5))

@recipe('Stab', 'vampstab', 14, ['Vamp Stab', 'Coffin Hit', 'Stab'], 'Rage WLR Carti Destroy_Lonely Dark Haunted', 'Vampire-rage synth stab with an organ body.')
def _(p):
    env_stab(p); p.osc(0, C('organ', 'hollow'), pos=U(0.2, 0.9), uni=3, det=0.2); p.osc(1, 'supersaw', pos=0.7, oct=1, uni=4, det=0.3, level=0.35)
    p.flt(1, 'ladder', cut=U(800, 2500, 0), reso=0.3, env=0.4, drive=0.4); chord(p, C('minor', 'phryg', 'dim')); dist(p, 'tube', U(0.3, 0.6), 0.6); return space(p, U(0.3, 0.7))

@recipe('Stab', 'pluckstab', 12, ['Chord Pluck', 'Stab', 'Pop Chord'], 'Hyperpop Bouncy Starboy Jetsonmade Chords Bright', 'Bright hyperpop chord pluck.')
def _(p):
    env_stab(p); p.osc(0, C('digital', 'pwm', 'bubble'), pos=U(0.2, 0.9), uni=C(2, 4), det=0.25, oct=1); p.flt(1, 'lp12', cut=U(2000, 6000, 0), reso=0.3, env=0.4)
    chord(p, C('maj7', 'madd9', 'sus2', 'min7')); pitch_drop(p, U(0.05, 0.2)); return space(p, U(0.2, 0.6))

# ---------------------------------------------------------------- ORCHESTRAL
@recipe('Orchestral', 'strings', 22, ['Strings', 'Violins', 'Ensemble', 'Orchestra'], 'Dark Trap Future TM88 Wide', 'Dark trap strings.')
def _(p):
    p.env(1, a=U(80, 400, 0), d=2000, sus=0.85, r=U(400, 1200, 0)); p.env(2, a=200, d=1500, sus=0.6, r=800)
    p.osc(0, 'supersaw', pos=U(0.3, 0.6), uni=C(5, 7), det=U(0.1, 0.2)); p.osc(1, 'basic', pos=0.66, oct=C(-1, 1), uni=3, det=0.12, level=0.4)
    p.flt(1, 'lp24', cut=U(1200, 3500, 0), reso=0.1, env=0.15); vibrato(p, 0.002, 5.0); chorus(p, 0.25); return space(p, U(0.5, 0.9))

@recipe('Orchestral', 'pizz', 14, ['Pizzicato', 'Pizz', 'Plucked Strings'], 'Bouncy Trap Jetsonmade TM88 Soft', 'Pizzicato strings for bouncy loops.')
def _(p):
    env_pluck(p, U(150, 350, 0)); p.osc(0, C('pluck', 'basic'), pos=U(0.3, 0.7)); p.osc(1, 'hollow', pos=0.4, oct=1, level=0.25)
    p.flt(1, 'lp24', cut=U(1500, 3500, 0), reso=0.15, env=0.4); return space(p, U(0.4, 0.8))

@recipe('Orchestral', 'brass', 14, ['Brass', 'Horns', 'Trumpets'], 'Dark Trap Future TM88 Bright', 'Bold brass section.')
def _(p):
    p.env(1, a=U(20, 60, 0), d=1200, sus=0.75, r=300); p.env(2, a=U(40, 100, 0), d=600, sus=0.5, r=300)
    p.osc(0, 'basic', pos=0.66, uni=C(3, 4), det=0.12); p.osc(1, 'pwm', pos=0.3, oct=-1, level=0.35)
    p.flt(1, 'lp24', cut=U(700, 1500, 0), reso=0.15, env=U(0.4, 0.6)); vibrato(p, 0.002); return space(p, U(0.4, 0.8))

@recipe('Orchestral', 'flute', 14, ['Flute', 'Piccolo', 'Pan Flute'], 'Dark Trap Future TM88 Plugg Soft', 'Airy flute section.')
def _(p):
    env_lead(p); p.osc(0, 'flute', pos=U(0, 0.6), oct=C(0, 1)); p.v.update(noise_on=1, noise_type=1, noise_level=U(0.08, 0.18), noise_route=0)
    p.flt(1, 'lp12', cut=U(3000, 7000, 0), reso=0.1); vibrato(p, U(0.003, 0.006)); return space(p, U(0.5, 0.95))

@recipe('Orchestral', 'choir', 12, ['Choir', 'Voices', 'Aahs'], 'Dark WLR TM88 Haunted Wide', 'Big dark choir "aahs".')
def _(p):
    env_pad(p); p.env(1, a=U(150, 500, 0), d=2000, sus=0.85, r=1200); p.osc(0, 'vocal', pos=U(0.0, 0.3), uni=6, det=0.18)
    p.flt(1, 'formant', cut=U(250, 600, 0), reso=0.55); chorus(p, 0.3); return space(p, U(0.6, 1.0))

@recipe('Orchestral', 'harp', 10, ['Harp', 'Concert Harp'], 'Ambient Melodic Destroy_Lonely Ginseng Dreamy', 'Concert harp.')
def _(p):
    env_pluck(p, U(700, 1500, 0)); p.osc(0, 'pluck', pos=U(0.2, 0.4)); p.osc(1, 'sinestack', pos=0.2, oct=1, level=0.25)
    p.flt(1, 'lp12', cut=U(2500, 5000, 0), reso=0.1, env=0.3, key=0.5); return space(p, U(0.5, 0.9))

@recipe('Orchestral', 'timpani', 8, ['Timpani', 'Drum Hit', 'Boom Hit'], 'Dark Trap Future Haunted', 'Cinematic low hit.')
def _(p):
    p.env(1, a=0.5, d=U(800, 1600, 0), sus=0, r=600); p.osc(0, 'basic', pos=0.0, oct=-1); pitch_drop(p, U(0.1, 0.25), U(40, 90, 0))
    p.v.update(noise_on=1, noise_type=1, noise_level=0.15, noise_route=0); p.env(3, a=0.5, d=60, sus=0, r=60); p.mod('env3', 'noise', 0.3)
    p.flt(1, 'lp24', cut=U(600, 1500, 0)); return space(p, U(0.6, 1.0))

# ---------------------------------------------------------------- VOX
@recipe('Vox', 'chop', 16, ['Vox', 'Chop', 'Vocal Chop'], 'Hyperpop Melodic Starboy Destroy_Lonely Dreamy', 'Vocal-chop-style formant pluck.')
def _(p):
    env_pluck(p, U(200, 500, 0)); p.osc(0, C('vocal', 'supersaw'), pos=U(0.2, 0.9), uni=C(1, 2, 3), det=0.15)
    p.flt(1, 'formant', cut=U(250, 900, 0), reso=U(0.3, 0.6), env=U(0.35, 0.6)); pitch_drop(p, U(0.02, 0.1)); return space(p, U(0.4, 0.8))

@recipe('Vox', 'ghost', 14, ['Ghost Vox', 'Whisper', 'Spirit Voice'], 'Dark Ambient Loesoe Ginseng Haunted', 'Ghostly breathing voice.')
def _(p):
    env_pad(p); p.osc(0, 'vocal', pos=U(0, 1), uni=3, det=0.25); p.v.update(noise_on=1, noise_type=1, noise_level=0.25, noise_route=0)
    p.flt(1, 'formant', cut=U(300, 1200, 0), reso=0.6); p.lfo(1, 'smooth', sync=0, rate=U(0.2, 0.8, 2), mode=0); p.mod('lfo1', 'cut1', 0.3); p.mod('lfo1', 'apos', 0.3)
    return space(p, U(0.7, 1.0))

@recipe('Vox', 'robot', 10, ['Robot', 'Talkbox', 'Vocoder'], 'Hyperpop Rage Starboy Ken_Carson Retro', 'Talkbox-ish robot voice lead.')
def _(p):
    env_lead(p); p.osc(0, 'supersaw', pos=0.9, uni=2, det=0.08); p.flt(1, 'formant', cut=U(400, 1200, 0), reso=0.6)
    p.lfo(1, C('square', 'stairs', 'sh'), div=C('1/8', '1/16')); p.mod('lfo1', 'cut1', U(0.2, 0.4)); mono(p, 40); return space(p, U(0.2, 0.6))

# ---------------------------------------------------------------- MOTION (Matrix + Minirack heavy)
@recipe('Motion', 'gated', 26, ['Gate', 'Chop', 'Pulse'], 'Rage Trap Hyperpop Ken_Carson Outtatown Moving', 'Sustained sound chopped by the Matrix.')
def _(p):
    env_pad(p); p.env(1, a=U(5, 60, 0), d=2000, sus=0.9, r=U(200, 700, 0))
    p.osc(0, C('supersaw', 'pwm', 'digital', 'organ', 'vocal', 'glass'), pos=U(0.2, 1.0), uni=C(3, 5, 7), det=U(0.15, 0.4))
    p.flt(1, 'lp24', cut=U(1200, 6000, 0), reso=0.2); p.tags_extra = flavour_matrix(p, ['Gate', 'Sidechain', 'Combo']); return space(p, U(0.3, 0.7))

@recipe('Motion', 'stutter', 18, ['Stutter', 'Glitch', 'Repeat'], 'Hyperpop Rage Osamason Bleood Moving Gritty', 'Lead mangled by the Matrix time lane.')
def _(p):
    env_lead(p); p.osc(0, C('digital', 'supersaw', 'chip', 'pwm'), pos=U(0, 1), uni=C(2, 4), det=0.25); p.flt(1, 'lp12', cut=U(3000, 9000, 0), reso=0.2)
    flavour_matrix(p, ['Time', 'Combo']); return space(p, U(0.2, 0.5))

@recipe('Motion', 'wobble', 14, ['Wobble', 'Sweep', 'Filter Motion'], 'Rage Hyperpop Bleood Gritty Moving', 'Filter lanes and wobbles from the Matrix.')
def _(p):
    env_lead(p); p.osc(0, C('supersaw', 'growl', 'basic'), pos=U(0.4, 1.0), uni=C(1, 3, 5), det=0.2); p.flt(1, 'lp24', cut=U(3000, 9000, 0), reso=0.1)
    flavour_matrix(p, ['Filter']); return space(p, U(0.2, 0.6))

@recipe('Motion', 'rackpad', 22, ['Living Pad', 'Organism', 'Swarm'], 'Ambient Clayco Ginseng Loesoe Moving Dreamy', 'Pad animated by the Minirack.')
def _(p):
    env_pad(p); p.osc(0, C('sweep', 'vocal', 'glass', 'hollow', 'bell'), pos=U(0, 1), uni=C(3, 4), det=0.2); p.osc(1, C('sinestack', 'glass'), pos=U(0, 1), oct=1, level=0.35)
    p.flt(1, 'lp24', cut=U(1000, 4000, 0), reso=0.2); flavour_rack(p, ['Movement', 'Subtle', 'Chaos', 'Random']); return space(p, U(0.6, 1.0))

@recipe('Motion', 'rackseq', 22, ['Sequence', 'Machine', 'Circuit'], 'Hyperpop Rage Osamason Bleood Moving Retro', 'The Minirack plays rhythms and melodies on top of your notes.')
def _(p):
    env_lead(p); p.osc(0, C('pwm', 'basic', 'digital', 'chip', 'pluck'), pos=U(0, 1), uni=C(1, 2, 3), det=0.15); p.osc(1, 'basic', pos=0.66, oct=C(0, 1), level=0.3)
    p.flt(1, 'lp24', cut=U(800, 3000, 0), reso=0.35); flavour_rack(p, ['Rhythmic', 'Random', 'Extreme']); return space(p, U(0.3, 0.7))

@recipe('Motion', 'pan', 12, ['Spinner', 'Orbit', 'Ping Pong'], 'Hyperpop Ambient Starboy Clayco Wide Moving', 'Bouncing around the stereo field.')
def _(p):
    env_pluck(p, U(300, 900, 0)); p.osc(0, C('glass', 'bubble', 'bell', 'pluck'), pos=U(0, 1), oct=C(0, 1)); p.flt(1, 'lp12', cut=U(2000, 6000, 0), reso=0.2)
    flavour_matrix(p, ['Pan']); return space(p, U(0.4, 0.8))

# ---------------------------------------------------------------- FX
@recipe('FX', 'riser', 12, ['Riser', 'Climb', 'Ascend'], 'Rage Hyperpop Trap Moving', 'Noise riser - hold a note and it builds.')
def _(p):
    p.osc(0, 'supersaw', pos=U(0.5, 0.9), uni=6, det=0.4); p.v.update(noise_on=1, noise_level=U(0.3, 0.6), noise_route=0)
    p.env(1, a=U(2000, 4000, 0), d=1000, sus=1, r=1500); p.env(3, a=U(3000, 5000, 0), d=100, sus=1, r=200, curve=-0.3)
    p.flt(1, 'lp24', cut=200, reso=0.4); p.mod('env3', 'cut1', 1.0); p.mod('env3', 'apitch', U(0.3, 0.6)); return space(p, U(0.6, 1.0))

@recipe('FX', 'zap', 10, ['Zap', 'Laser', 'Blaster'], 'Hyperpop Rage Starboy Retro', 'Laser zap - a quick pitch dive.')
def _(p):
    p.osc(0, C('basic', 'chip', 'digital'), pos=U(0.3, 1)); p.env(3, a=0.5, d=U(80, 250, 0), sus=0, r=50, curve=0.5); p.mod('env3', 'apitch', U(0.6, 1.0))
    p.env(1, a=0.5, d=300, sus=0, r=100); p.flt(1, 'lp12', cut=9000); return space(p, U(0.3, 0.7))

@recipe('FX', 'texture', 12, ['Texture', 'Swarm', 'Crawl'], 'Ambient Dark Clayco Loesoe Haunted Moving', 'Granular-style wavetable texture.')
def _(p):
    p.osc(0, C('vocal', 'sweep', 'growl', 'bell'), pos=U(0, 1), uni=4, det=0.3); p.lfo(1, 'smooth', sync=0, rate=U(0.3, 2, 2), mode=0); p.mod('lfo1', 'apos', 0.6)
    p.env(1, a=800, d=2000, sus=0.8, r=2500); p.flt(1, 'bp', cut=U(800, 3000, 0), reso=0.4); return space(p, U(0.7, 1.0))

@recipe('FX', 'impact', 8, ['Impact', 'Hit', 'Drop'], 'Trap Rage Dark Gritty', 'Big downlifter impact.')
def _(p):
    p.osc(0, 'basic', pos=0.66, uni=4, det=0.4); p.v.update(noise_on=1, noise_level=0.4, noise_route=0)
    p.env(1, a=0.5, d=U(1500, 3000, 0), sus=0, r=800); p.env(3, a=0.5, d=U(800, 1500, 0), sus=0, r=100, curve=0.2); p.mod('env3', 'apitch', U(0.4, 0.8))
    p.flt(1, 'lp24', cut=U(2000, 6000, 0), reso=0.2); dist(p, 'overdrive', 0.4, 0.5); return space(p, U(0.7, 1.0))

@recipe('FX', 'drone', 8, ['Drone', 'Hum', 'Engine'], 'Dark Ambient Haunted', 'Low evolving drone.')
def _(p):
    env_pad(p); p.osc(0, C('growl', 'hollow', 'sweep'), pos=U(0, 1), oct=-1, uni=4, det=0.3); p.flt(1, 'lp24', cut=U(300, 900, 0), reso=0.4)
    flavour_rack(p, ['Chaos', 'Subtle']); return space(p, U(0.7, 1.0))

# ================================================================ global flavours (keep the bank varied)
def global_flavours(p, cat):
    if cat in ('Pad', 'Keys', 'Bell', 'Orchestral') and P(0.12) and not p.v.get('mx_on'): flavour_matrix(p, ['Gate', 'Sidechain', 'Filter', 'Pan'])
    if cat in ('Lead', 'Pluck', 'Arp', 'Stab') and P(0.08) and not p.v.get('mx_on'): flavour_matrix(p, ['Gate', 'Pan', 'Time', 'Combo'])
    if cat in ('Pad', 'Lead', 'Bell', 'Keys', 'Vox') and P(0.15) and not p.v.get('rk_on'): flavour_rack(p, ['Movement', 'Subtle', 'Random', 'Follower'])
    if cat not in ('Bass', 'FX') and P(0.25) and 'tape_on' not in p.v: tape(p)
    if cat not in ('Bass',) and P(0.3): drift(p)
    if P(0.2) and 'ch_on' not in p.v and cat not in ('Bass',): chorus(p)
    if P(0.08) and cat not in ('Bass',): phaser(p)
    if P(0.06) and cat not in ('Bass',): flanger(p)
    if P(0.2): eq(p)
    if P(0.12) and cat in ('Lead', 'Stab', 'Bass'): comp(p)


# ================================================================ XOST: styles from the wider scene (see README "Research")
XOST_NEW[0] = True
# ---- phonk (Memphis + drift): melodic distorted 808 cowbell, blown-out bass, lo-fi keys
@recipe('Bell', 'cowbell', 24, ['Cowbell', 'Bell', 'Clank', 'Drift Bell'], 'Phonk Dark Kordhell Gritty LoFi', 'Melodic 808-style cowbell, crushed and driven - the phonk lead.')
def _(p):
    p.env(1, a=0.5, d=U(180, 650, 0), sus=0.0, r=U(120, 400, 0), curve=U(0.5, 0.85)); p.env(2, a=0.5, d=U(60, 200, 0), sus=0, r=100)
    p.osc(0, 'basic', pos=C(1.0, 0.95, 0.9)); p.osc(1, 'basic', pos=1.0, semi=7, fine=U(-24, -16, 1), level=U(0.55, 0.8))
    p.flt(1, 'bp', cut=U(1200, 3200, 0), reso=U(0.3, 0.55), env=U(0.1, 0.3), key=0.6)
    dist(p, C('hard', 'crush', 'overdrive'), U(0.5, 0.9), U(0.6, 1.0)); maybe(p, 0.5, lambda q: tape(q, U(0.2, 0.6)))
    return space(p, U(0.1, 0.45))

@recipe('Bass', 'phonk', 20, ['Phonk 808', 'Blown 808', 'Drift 808', 'Clipper'], 'Phonk Kordhell Gritty Mono', 'Blown-out, clipped phonk 808 that distorts on purpose.')
def _(p):
    bass_base(p); p.osc(0, 'basic', pos=C(0.0, 0.1, 0.2), oct=-1); pitch_drop(p, U(0.2, 0.45), U(20, 45, 0))
    p.v.update(shp_on=1, shp_type=C(2, 3), shp_drive=U(0.7, 1.0), shp_pos=1); dist(p, C('hard', 'fold', 'crush'), U(0.6, 1.0), U(0.7, 1.0))
    p.flt(1, 'lp24', cut=U(1800, 6000, 0), reso=0.1); p.set(sub_on=1, sub_shape=0, sub_oct=2, sub_level=U(0.4, 0.7), sub_route=3)
    p.env(1, a=1, d=U(600, 1600, 0), sus=0.0, r=U(100, 250, 0), curve=0.2); return p

@recipe('Keys', 'memphis', 14, ['Memphis Keys', 'Cassette Keys', 'Basement Piano'], 'Phonk Dark LoFi Haunted', 'Dusty Memphis-style keys through a phone-speaker EQ and tape.')
def _(p):
    env_keys(p); p.osc(0, C('sinestack', 'organ', 'hollow'), pos=U(0.1, 0.6)); p.osc(1, 'pluck', pos=0.4, oct=1, level=0.25)
    p.flt(1, 'bp', cut=U(900, 1800, 0), reso=0.2, key=0.3); tape(p, U(0.6, 1.0)); p.fx('dist', type=DIST['crush'], drive=U(0.2, 0.5), tone=0.4, mix=U(0.3, 0.6))
    maybe(p, 0.4, lambda q: chord(q, C('minor', 'min7', 'phryg'))); p.set(polyphony=24); return space(p, U(0.3, 0.7))

# ---- Yeat / Synthetic / Bnyx: tolling metallic bells over rage drums
@recipe('Bell', 'toll', 22, ['Toll', 'Bell', 'Church Bell', 'Tower'], 'Rage Yeat Bnyx Dark Glassy Wide', 'Sharp, echoing tolling bell with a metallic edge.')
def _(p):
    env_bell(p); fm_pair(p, C(5, 17, 18, 13), U(0.25, 0.55), 'fm', 'basic', 0.0); p.mod('env2', 'amod', U(0.3, 0.6))
    p.osc(2, C('bell', 'glass'), pos=U(0.2, 0.8), oct=1, level=U(0.15, 0.35)); p.flt(1, 'lp12', cut=U(3500, 9000, 0), reso=0.1)
    maybe(p, 0.6, lambda q: dist(q, C('tube', 'overdrive'), U(0.15, 0.4), U(0.2, 0.5)))
    return space(p, U(0.6, 1.0))

@recipe('Lead', 'yeat', 16, ['Bell Lead', 'Chime Lead', 'Ring Lead'], 'Rage Yeat Bnyx Bright Glassy', 'Bright chime lead built for Yeat-style hooks.')
def _(p):
    env_lead(p); p.env(1, a=0.5, d=U(500, 1400, 0), sus=U(0.25, 0.5), r=U(300, 900, 0)); fm_pair(p, C(12, 19, 24), U(0.15, 0.35), 'pm', C('glass', 'bell'), U(0.1, 0.6))
    p.osc(2, 'supersaw', pos=0.85, uni=5, det=0.2, level=U(0.1, 0.25)); p.flt(1, 'lp12', cut=U(4000, 10000, 0), reso=0.1)
    return space(p, U(0.4, 0.8))

# ---- F1lthy / Working on Dying: degraded "mp3" synths, layered and crushed
@recipe('Lead', 'mp3', 16, ['MP3 Lead', 'Lossy', 'Bootleg', 'Rip'], 'Rage F1lthy WLR Carti LoFi Gritty Detuned', 'Lead that sounds like a degraded mp3 - crushed, warbly and wide.')
def _(p):
    env_lead(p); p.osc(0, 'supersaw', pos=U(0.6, 1.0), uni=C(5, 7), det=U(0.2, 0.4)); p.osc(1, C('digital', 'chip'), pos=U(0, 1), oct=1, level=U(0.2, 0.4))
    p.flt(1, 'lp12', cut=U(2500, 6000, 0), reso=0.2); p.fx('dist', type=DIST['crush'], drive=U(0.35, 0.7), tone=U(0.3, 0.6), mix=U(0.4, 0.8))
    tape(p, U(0.5, 0.9)); p.fx('ph', rate=U(0.1, 0.4), depth=0.5, fb=0.3, mix=0.25); return space(p, U(0.2, 0.6))

# ---- pluggnb (Summrs, Autumn!, XanGang, ARIZONATEARS): EP block chords with 7ths/9ths, sine leads, glassy bells
@recipe('Keys', 'pluggnb', 26, ['Pluggnb Keys', 'Velvet EP', 'Love Keys', 'Block Chords'], 'Pluggnb Plugg Melodic Summrs Autumn Chords Soft Dreamy', 'Electric piano that plays jazzy pluggnb block chords (min9 / add9) from one key.')
def _(p):
    env_keys(p); fm_pair(p, C(12, 24), U(0.08, 0.22), 'pm', C('sinestack', 'basic'), U(0.0, 0.3)); p.flt(1, 'lp12', cut=U(2000, 4500, 0), reso=0.1)
    chorus(p, U(0.2, 0.45)); chord(p, C('min9', 'madd9', 'maj9', 'min7', 'maj7'), voicing=C(0, 1, 1), inv=C(0, 1, 2), strum=U(0, 25, 0))
    maybe(p, 0.6, lambda q: tape(q, U(0.2, 0.6))); p.set(polyphony=32); return space(p, U(0.4, 0.85))

@recipe('Lead', 'pluggnbsine', 18, ['Sine Lead', 'Whistle', 'Lullaby Lead'], 'Pluggnb Plugg Summrs Autumn Soft Dreamy Mono', 'Pure sine lead with glide and vibrato - pluggnb toplines.')
def _(p):
    env_lead(p); p.osc(0, 'basic', pos=C(0.0, 0.05, 0.15)); maybe(p, 0.5, lambda q: q.osc(1, 'sinestack', pos=U(0, 0.4), oct=1, level=0.15))
    p.flt(1, 'lp12', cut=U(3000, 8000, 0), reso=0.05); mono(p, U(60, 160, 0)); vibrato(p, U(0.003, 0.006)); return space(p, U(0.5, 0.95))

@recipe('Bell', 'pluggnb', 16, ['Glass Bell', 'Heart Bell', 'Sugar Bell'], 'Pluggnb Plugg Summrs Glassy Soft Dreamy', 'Glassy pluggnb bell that sits on top of the chords.')
def _(p):
    env_bell(p); p.osc(0, C('glass', 'sinestack', 'bell'), pos=U(0.1, 0.7), oct=1); p.osc(1, 'basic', pos=0.0, oct=2, level=U(0.1, 0.25))
    p.flt(1, 'lp12', cut=U(3000, 8000, 0), reso=0.1); maybe(p, 0.5, lambda q: chorus(q, 0.3)); return space(p, U(0.5, 0.9))

# ---- jerk (xaviersobased, kashpaint, Nettspend): twitchy, degraded FX-spam, sidechained bass
@recipe('Lead', 'jerk', 16, ['Jerk Lead', 'Twitch', 'Spaz', 'Glitch Lead'], 'Jerk Xaviersobased Nettspend LoFi Gritty Moving', 'Degraded, twitchy jerk lead with pitch zaps.')
def _(p):
    env_lead(p); p.osc(0, C('digital', 'chip', 'pwm', 'vocal'), pos=U(0, 1), uni=C(1, 2, 3), det=0.2); p.flt(1, 'lp12', cut=U(2000, 6000, 0), reso=0.25)
    p.lfo(2, 'sh', div=C('1/16', '1/32', '1/16T')); p.mod('lfo2', 'apitch', C(0.02, 0.04, 0.08)); p.mod('lfo2', 'cut1', 0.2)
    p.fx('dist', type=DIST['crush'], drive=U(0.4, 0.85), tone=U(0.3, 0.7), mix=U(0.5, 0.9)); return space(p, U(0.1, 0.5))

@recipe('Bass', 'jerk', 14, ['Pump Bass', 'Jerk Bass', 'Duck Bass'], 'Jerk Xaviersobased Moving Mono Gritty', 'Fat bass that ducks on every beat, like it is sidechained to the kick.')
def _(p):
    bass_base(p); p.osc(0, C('basic', 'pwm'), pos=U(0.4, 0.9), oct=-1); p.flt(1, 'lp24', cut=U(300, 900, 0), reso=0.25)
    p.env(1, a=1, d=3000, sus=0.85, r=200); p.v.update(shp_on=1, shp_type=1, shp_drive=U(0.3, 0.7))
    p.matrix({0: lane_pump(4, depth=U(0.7, 1.0))}, lens={0: '1bar'}); return p

@recipe('FX', 'fxspam', 14, ['FX Spam', 'Zap Spam', 'Sfx', 'Blips'], 'Jerk Sigilkore Hyperpop Xaviersobased Moving Retro', 'Hold a note: a stream of degraded blips and zaps.')
def _(p):
    p.env(1, a=0.5, d=400, sus=0.8, r=200); p.osc(0, C('chip', 'digital', 'bubble'), pos=U(0, 1)); p.flt(1, 'lp12', cut=U(3000, 9000, 0))
    p.rack([{'type': 'sh', 'div': C('1/16', '1/32'), 'a': 0.0, 'b': 0.5, 'dst': 'apitch', 'amt': U(0.08, 0.2), 'dst2': 'apos', 'amt2': 0.5},
            {'type': 'turing', 'div': '1/16', 'a': 0.5, 'b': 0.6, 'dst': 'amp', 'amt': -0.6}])
    p.fx('dist', type=DIST['crush'], drive=U(0.4, 0.8), tone=0.5, mix=0.7); return space(p, U(0.2, 0.5))

# ---- sigilkore (Luci4, Islurwhenitalk): bitcrushed + pitch-shifted, ambient but fast
@recipe('Pad', 'sigil', 14, ['Sigil Pad', 'Crushed Haze', 'Emerald Fog'], 'Sigilkore Luci4 Ambient Dark LoFi Haunted', 'Bitcrushed ambient pad drowned in reverb.')
def _(p):
    env_pad(p); p.osc(0, C('vocal', 'supersaw', 'glass'), pos=U(0, 1), uni=C(4, 6), det=U(0.25, 0.45)); p.flt(1, 'lp12', cut=U(1500, 5000, 0), reso=0.2)
    p.fx('dist', type=DIST['crush'], drive=U(0.45, 0.85), tone=U(0.3, 0.6), mix=U(0.4, 0.8)); tape(p, U(0.4, 0.8)); return space(p, 1.0)

@recipe('Lead', 'sigil', 14, ['Sigil Lead', 'Kurse', 'Jewel'], 'Sigilkore Luci4 Dark LoFi Gritty', 'Pitch-shifted, crushed lead with a halo of reverb.')
def _(p):
    env_lead(p); p.osc(0, C('supersaw', 'vocal'), pos=U(0.4, 1.0), oct=1, uni=C(3, 5), det=0.3); p.osc(1, 'basic', pos=0.66, oct=0, level=0.3)
    p.flt(1, 'lp24', cut=U(2500, 7000, 0), reso=0.2); p.fx('dist', type=DIST['crush'], drive=U(0.5, 0.9), tone=0.5, mix=U(0.5, 0.9))
    return space(p, U(0.7, 1.0))

# ---- digicore / glitchcore (glaive, ericdoa): bright pluggnb-ish melodies, autotune-y leads, sharp 808s
@recipe('Pluck', 'digicore', 16, ['Digi Pluck', 'Pixel Pluck', 'Shimmer'], 'Digicore Hyperpop Glaive Bright Glassy Detuned', 'Bright digicore pluck, sparkly and a bit detuned.')
def _(p):
    env_pluck(p); p.osc(0, C('supersaw', 'glass', 'digital'), pos=U(0.5, 1.0), oct=1, uni=C(3, 5), det=U(0.15, 0.3)); p.osc(1, 'bell', pos=U(0, 0.5), oct=2, level=0.2)
    p.flt(1, 'lp12', cut=U(2500, 7000, 0), reso=0.2, env=0.4); maybe(p, 0.4, lambda q: chorus(q)); return space(p, U(0.3, 0.7))

@recipe('Lead', 'autotune', 14, ['Tune Lead', 'Robot Voice', 'Snap Lead'], 'Digicore Hyperpop Glaive Bright Mono Retro', 'Hard-tuned vocal-ish lead with instant glide.')
def _(p):
    env_lead(p); p.osc(0, 'vocal', pos=U(0.2, 0.8), uni=2, det=0.08); p.flt(1, 'formant', cut=U(500, 1400, 0), reso=0.5)
    p.flt(2, 'lp12', cut=U(5000, 10000, 0)); p.set(voice_mode=2, glide=U(15, 40, 0), glide_mode=1); return space(p, U(0.3, 0.6))

# ---- Drain Gang / Whitearmor / cloud rap (Clams Casino): soft fluid trance synths, reverb-drenched vocal pads
@recipe('Pad', 'trance', 18, ['Trance Pad', 'Eternal', 'Heaven', 'Angelic'], 'Cloud Ambient Drain_Gang Melodic Dreamy Wide Detuned', 'Soft trance supersaw pad, glossy and floating.')
def _(p):
    env_pad(p); p.env(1, a=U(80, 600, 0), d=2000, sus=0.85, r=U(1200, 3000, 0)); p.osc(0, 'supersaw', pos=U(0.6, 1.0), uni=C(7, 8), det=U(0.2, 0.35), width=1.0)
    p.flt(1, 'lp12', cut=U(1800, 5000, 0), reso=0.15); chorus(p, U(0.3, 0.5)); return space(p, U(0.8, 1.0))

@recipe('Pluck', 'drain', 16, ['Fluid Pluck', 'Glow', 'Angel Pluck'], 'Cloud Drain_Gang Ambient Melodic Soft Dreamy', 'Soft, fluid pluck with long delays - Drain Gang-style.')
def _(p):
    env_pluck(p, U(300, 800, 0)); p.osc(0, C('supersaw', 'glass', 'sinestack'), pos=U(0.3, 0.9), uni=C(1, 3, 5), det=0.15)
    p.flt(1, 'lp12', cut=U(1500, 4000, 0), reso=0.15, env=0.35); chorus(p, 0.35)
    p.fx('dly', l=DIV['1/4.'], r=DIV['1/4'], fb=U(0.4, 0.6), ping=0.8, tone=0.5, mix=U(0.2, 0.35)); return space(p, U(0.7, 1.0))

@recipe('Pad', 'cloud', 16, ['Cloud', 'Based Pad', 'Vapor Choir', 'Heaven Chop'], 'Cloud Ambient Clams_Casino Dreamy Haunted Wide', 'Pitched, reverb-drenched vocal pad - cloud rap haze.')
def _(p):
    env_pad(p); p.osc(0, 'vocal', pos=U(0, 1), uni=C(3, 5), det=0.3); p.osc(1, 'sinestack', pos=0.3, oct=-1, level=0.3)
    p.flt(1, 'formant', cut=U(400, 1200, 0), reso=0.5); p.flt(2, 'lp12', cut=U(2500, 6000, 0)); tape(p, U(0.3, 0.7))
    p.lfo(1, 'smooth', sync=0, rate=U(0.1, 0.5, 2), mode=0); p.mod('lfo1', 'cut1', 0.25); return space(p, 1.0)

# ---- Jersey club: chopped vocal stabs + the "bed squeak"
@recipe('Vox', 'jersey', 16, ['Club Chop', 'Hey Chop', 'Jersey Vox'], 'Jersey_Club Bouncy Hyperpop Bright', 'Short chopped vocal stab for Jersey club bounce.')
def _(p):
    p.env(1, a=0.5, d=U(90, 220, 0), sus=0.0, r=U(40, 120, 0), curve=0.4); p.osc(0, 'vocal', pos=U(0.1, 0.9), uni=2, det=0.1)
    p.flt(1, 'formant', cut=U(500, 1500, 0), reso=U(0.4, 0.7)); pitch_drop(p, U(0.03, 0.12), U(15, 40, 0)); return space(p, U(0.1, 0.4))

@recipe('FX', 'squeak', 10, ['Squeak', 'Chirp', 'Bed Squeak'], 'Jersey_Club Bouncy Retro', 'Rubbery pitched squeak - play it on the off-beats.')
def _(p):
    p.env(1, a=0.5, d=U(80, 180, 0), sus=0, r=60); p.osc(0, C('vocal', 'hollow', 'bubble'), pos=U(0.2, 0.8), oct=1)
    p.env(3, a=U(20, 60, 0), d=120, sus=0, r=50, curve=-0.3); p.mod('env3', 'apitch', U(0.05, 0.12))
    p.flt(1, 'bp', cut=U(1500, 3500, 0), reso=0.6); return p

# ---- drill (UK / NY / Chicago) + Detroit: sliding 808s and halftime dark piano
@recipe('Bass', 'drill', 22, ['Drill 808', 'Slide 808', 'Glide', 'Slider'], 'Drill Detroit M1onthebeat Dark Mono', 'Punchy 808 with long slides between overlapping notes - for drill.')
def _(p):
    bass_base(p); p.osc(0, 'basic', pos=C(0.0, 0.05), oct=-1); pitch_drop(p, U(0.08, 0.2), U(25, 45, 0))
    p.set(voice_mode=2, glide=U(90, 260, 0), glide_mode=0); p.v.update(shp_on=1, shp_type=C(0, 1), shp_drive=U(0.3, 0.6), shp_pos=1)
    p.flt(1, 'lp24', cut=U(900, 2500, 0), reso=0.05); p.set(sub_on=1, sub_shape=0, sub_oct=2, sub_level=U(0.3, 0.55), sub_route=3)
    p.env(1, a=1, d=U(2000, 4000, 0), sus=0.0, r=U(200, 400, 0)); return p

@recipe('Keys', 'drill', 18, ['Drill Piano', 'Halftime Keys', 'Block'], 'Drill Detroit Dark Haunted Soft', 'Dark halftime drill piano with a long cold tail.')
def _(p):
    env_keys(p); p.env(1, a=0.5, d=U(1500, 3500, 0), sus=0.1, r=U(600, 1500, 0), curve=0.5); p.osc(0, 'sinestack', pos=U(0.3, 0.7)); p.osc(1, 'pluck', pos=0.4, oct=1, level=0.3)
    p.flt(1, 'lp12', cut=U(1800, 4500, 0), reso=0.1, env=0.2, key=0.5); p.set(polyphony=24); return space(p, U(0.6, 1.0))

@recipe('Keys', 'detroit', 18, ['Detroit Keys', 'Motor Piano', 'Eastside'], 'Detroit Drill Dark LoFi Soft', 'Minor-key Detroit piano with a lo-fi edge.')
def _(p):
    env_keys(p); p.osc(0, C('sinestack', 'pluck'), pos=U(0.2, 0.6)); p.osc(1, 'basic', pos=0.33, oct=1, level=0.2)
    p.flt(1, 'lp12', cut=U(1500, 3500, 0), reso=0.15, env=0.3); tape(p, U(0.25, 0.6)); p.set(polyphony=24); return space(p, U(0.4, 0.8))

@recipe('Stab', 'glo', 14, ['Glo Horn', 'Brass Hit', 'Chopper Horn'], 'Drill Dark Trap Gritty Bright', 'Bold brassy horn stab - Chicago drill / glo energy.')
def _(p):
    env_stab(p); p.env(1, a=U(5, 20, 0), d=U(300, 700, 0), sus=0.3, r=200); p.osc(0, 'basic', pos=0.66, uni=3, det=0.12); p.osc(1, 'pwm', pos=0.5, oct=-1, level=0.4)
    p.flt(1, 'lp24', cut=U(900, 2200, 0), reso=0.2, env=0.5); maybe(p, 0.5, lambda q: chord(q, C('power', 'minor', 'oct')))
    dist(p, 'tube', U(0.2, 0.5), 0.5); return space(p, U(0.2, 0.5))

# ---- sexy drill (Cash Cobain): smooth R&B keys and sample-flip textures
@recipe('Keys', 'rnb', 18, ['R&B Keys', 'Slizzy Keys', 'Silk EP', 'Late Night'], 'Sexy_Drill RnB Cash_Cobain Melodic Soft Chords', 'Smooth R&B electric piano with chorus and a warm tape glow.')
def _(p):
    env_keys(p); fm_pair(p, C(12, 24, 19), U(0.1, 0.25), 'pm', 'sinestack', U(0.0, 0.4)); p.flt(1, 'lp12', cut=U(1800, 4000, 0), reso=0.1)
    chorus(p, U(0.3, 0.55)); tape(p, U(0.3, 0.6)); maybe(p, 0.6, lambda q: chord(q, C('maj9', 'min9', 'maj7', 'min7'), voicing=1, strum=U(0, 30, 0)))
    p.lfo(4, 'sine', sync=0, rate=U(3, 5, 2)); p.mod('lfo4', 'amp', 0.1); p.set(polyphony=32); return space(p, U(0.4, 0.8))

@recipe('Vox', 'sampleflip', 14, ['Sample Flip', 'Soul Chop', 'Pitched Chop'], 'Sexy_Drill RnB Cloud Cash_Cobain Clams_Casino LoFi Dreamy', 'Pitched soul-sample-style vocal chop with tape warble.')
def _(p):
    env_pluck(p, U(400, 900, 0)); p.osc(0, 'vocal', pos=U(0, 1), uni=3, det=0.15); p.osc(1, 'sinestack', pos=0.4, level=0.25)
    p.flt(1, 'formant', cut=U(400, 1200, 0), reso=0.5); tape(p, U(0.5, 0.9)); return space(p, U(0.5, 0.9))

# ---- Metro Boomin / Southside / Wheezy: cinematic dark keys, ghost strings, flute
@recipe('Orchestral', 'ghost', 16, ['Ghost Strings', 'Horror Strings', 'Tremolo'], 'Dark Trap Metro_Boomin Southside Haunted Wide', 'Eerie tremolo strings - cinematic, film-score dark trap.')
def _(p):
    env_pad(p); p.env(1, a=U(80, 400, 0), d=2000, sus=0.8, r=U(800, 2000, 0)); p.osc(0, 'supersaw', pos=U(0.5, 0.8), uni=C(5, 7), det=0.2)
    p.osc(1, 'basic', pos=0.66, oct=1, level=0.3); p.flt(1, 'lp24', cut=U(1200, 3000, 0), reso=0.15)
    p.lfo(3, 'sine', div=C('1/16', '1/32', '1/16T')); p.mod('lfo3', 'amp', U(0.3, 0.6)); return space(p, U(0.6, 1.0))

@recipe('Bell', 'metro', 18, ['Dark Bells', 'Grave Bells', 'Hymn Bells'], 'Dark Trap Metro_Boomin Southside Wheezy Haunted Glassy', 'Haunting minor-key bells with a cinematic tail.')
def _(p):
    env_bell(p); fm_pair(p, C(12, 24, 31), U(0.12, 0.3), 'fm', 'sinestack', 0.2); p.osc(2, 'bell', pos=U(0.3, 0.8), oct=-1, level=0.2)
    p.flt(1, 'lp12', cut=U(2500, 6000, 0), reso=0.1); return space(p, U(0.6, 1.0))

@recipe('Keys', 'cinematic', 16, ['Film Piano', 'Score Keys', 'Requiem Piano'], 'Dark Trap Metro_Boomin Southside Haunted Soft', 'Big dark cinematic piano for two-note trap loops.')
def _(p):
    env_keys(p); p.env(1, a=0.5, d=U(2500, 4500, 0), sus=0.15, r=U(1200, 2500, 0), curve=0.5); p.osc(0, 'sinestack', pos=U(0.4, 0.8)); p.osc(1, 'basic', pos=0.0, oct=-1, level=0.3)
    p.flt(1, 'lp12', cut=U(1500, 3500, 0), reso=0.1, key=0.5); p.set(polyphony=24); return space(p, U(0.7, 1.0))

# ---- emo rap (Lil Peep, Juice WRLD, GothBoiClique): sad guitar plucks and strums
@recipe('Pluck', 'emo', 18, ['Emo Guitar', 'Sad Strum', 'Clean Guitar', 'Bedroom Guitar'], 'Emo Melodic Lil_Peep Soft Chords LoFi', 'Clean, sad guitar-style pluck - strums a chord from one key.')
def _(p):
    env_pluck(p, U(500, 1200, 0)); p.osc(0, 'pluck', pos=U(0.2, 0.7)); p.osc(1, 'hollow', pos=0.3, oct=0, fine=U(4, 9, 1), level=0.3)
    p.flt(1, 'lp24', cut=U(1500, 4000, 0), reso=0.2, env=0.4, key=0.5); chorus(p, U(0.25, 0.45))
    maybe(p, 0.7, lambda q: chord(q, C('minor', 'madd9', 'sus2', 'maj7', 'power'), voicing=C(0, 1), strum=U(20, 70, 0)))
    maybe(p, 0.5, lambda q: tape(q, U(0.2, 0.5))); return space(p, U(0.4, 0.8))

@recipe('Lead', 'emo', 12, ['Grunge Lead', 'Fuzz Guitar', 'Power Chord'], 'Emo Rage Lil_Peep Gritty', 'Fuzzy distorted guitar-ish lead.')
def _(p):
    env_lead(p); p.osc(0, 'pluck', pos=U(0.4, 0.9), uni=2, det=0.1); p.osc(1, 'basic', pos=0.66, oct=-1, level=0.4)
    p.flt(1, 'lp24', cut=U(2000, 4500, 0), reso=0.2); dist(p, C('fold', 'hard', 'tube'), U(0.6, 0.95), 0.9); return space(p, U(0.2, 0.5))

# ---- extreme rage / horror (Osamason, Che, Sematary, Haunted Mound): blown-out leads and horror textures
@recipe('Lead', 'horror', 14, ['Horror Lead', 'Graveyard', 'Butcher', 'Coffin Lead'], 'Rage Dark Sematary Osamason Che Haunted Gritty LoFi', 'Blown-out horror lead - distorted, detuned, unsettling.')
def _(p):
    env_lead(p); p.osc(0, C('organ', 'growl', 'hollow'), pos=U(0, 1), uni=C(3, 5), det=U(0.3, 0.5)); p.osc(1, 'basic', pos=0.66, semi=C(1, 6, 11), level=U(0.2, 0.35))
    p.flt(1, 'lp24', cut=U(1500, 4000, 0), reso=0.3, drive=0.5); dist(p, C('hard', 'fold', 'crush'), U(0.6, 1.0), U(0.7, 1.0)); tape(p, U(0.4, 0.8))
    return space(p, U(0.3, 0.7))

# ---- hyperpop (SOPHIE, A. G. Cook, umru): plastic, metallic percussive synths
@recipe('Pluck', 'plastic', 14, ['Plastic', 'Latex', 'Bubblegum', 'Rubber'], 'Hyperpop Sophie AG_Cook Bright Glassy Bouncy', 'Glossy, rubbery metallic pluck - PC Music / SOPHIE style.')
def _(p):
    env_pluck(p, U(120, 350, 0)); fm_pair(p, C(7, 12, 17, 19), U(0.3, 0.6), 'fm'); p.mod('env2', 'amod', U(0.3, 0.6)); pitch_drop(p, U(0.15, 0.4), U(10, 30, 0))
    p.flt(1, 'peak', cut=U(1500, 4000, 0), reso=0.5); return space(p, U(0.2, 0.5))

@recipe('Bass', 'hyper', 10, ['Hyper Bass', 'Squish', 'Slime'], 'Hyperpop Sophie AG_Cook Gritty Mono', 'Squelchy, over-compressed hyperpop bass.')
def _(p):
    bass_base(p); p.osc(0, C('growl', 'digital'), pos=U(0, 1), oct=-1); p.flt(1, 'ladder', cut=U(400, 1200, 0), reso=U(0.4, 0.7), env=0.5)
    p.env(2, a=0.5, d=U(100, 300, 0), sus=0.1, r=100); comp(p); dist(p, 'hard', U(0.3, 0.6), 0.6); return p

def finish_tags(p, base_tags):
    tags = base_tags.split()
    if p.v.get('mx_on') or p.v.get('rk_on'): tags.append('Moving')
    if p.v.get('chord_type'): tags.append('Chords')
    if p.v.get('arp_on'): tags.append('Arp')
    if p.v.get('tape_on'): tags.append('LoFi')
    seen = []
    for t in tags:
        if t not in seen: seen.append(t)
    return ' '.join(seen)

TYPE_NAMES = {'Carti': 'Playboi Carti', 'WLR': 'Whole Lotta Red', 'Ken_Carson': 'Ken Carson', 'Destroy_Lonely': 'Destroy Lonely',
              'Osamason': 'Osamason', 'Che': 'Che', 'Future': 'Future', 'Uzi': 'Lil Uzi Vert', 'Bleood': 'bleood', 'Jetsonmade': 'Jetsonmade',
              'TM88': 'TM88', 'Outtatown': 'Outtatown', 'Starboy': 'StarBoy', 'Richie_Souf': 'Richie Souf', 'Pierre_Bourne': 'Pierre Bourne',
              'Clayco': 'Clayco', 'Loesoe': 'Loesoe', 'Ginseng': 'Ginseng',
              'Yeat': 'Yeat', 'Bnyx': 'Bnyx', 'F1lthy': 'F1lthy', 'Summrs': 'Summrs', 'Autumn': 'Autumn!', 'Xaviersobased': 'xaviersobased',
              'Nettspend': 'Nettspend', 'Luci4': 'Luci4', 'Glaive': 'glaive', 'Drain_Gang': 'Drain Gang', 'Clams_Casino': 'Clams Casino',
              'Kordhell': 'Kordhell', 'M1onthebeat': 'M1OnTheBeat', 'Cash_Cobain': 'Cash Cobain', 'Metro_Boomin': 'Metro Boomin',
              'Southside': 'Southside', 'Wheezy': 'Wheezy', 'Lil_Peep': 'Lil Peep', 'Sematary': 'Sematary', 'Sophie': 'SOPHIE', 'AG_Cook': 'A. G. Cook'}

def describe(desc, tags, p):
    types = [TYPE_NAMES[t] for t in tags.split() if t in TYPE_NAMES]
    extra = []
    if p.v.get('mx_on'): extra.append('Matrix on')
    if p.v.get('rk_on'): extra.append('Minirack on')
    out = desc
    if types: out += ' For %s-style beats (our interpretation).' % ' / '.join(types[:3])
    if extra: out += ' ' + ', '.join(extra) + '.'
    return out

# ================================================================ build
def build_one(rec, i, attempt):
    R.seed(hash((rec['cat'], rec['key'], i, attempt)) & 0xffffffff)
    p = Patch('?', rec['cat'], '', '')
    p.name = name_for(rec['nouns'])
    rec['fn'](p)
    global_flavours(p, rec['cat'])
    macros_std(p)
    p.tags = finish_tags(p, rec['tags'])
    p.desc = describe(rec['desc'], p.tags, p)
    return p

FACTORY_PER_RECIPE = 3
def build_bank():
    """Returns (seeds, factory). Seeds = every recipe variation (the word engine's sound library);
    factory = ~3 per recipe, the browsable presets (loudness-calibrated)."""
    import zlib
    seeds, factory = [], []
    init = Patch('Init', 'Init', '', 'Clean starting point: one saw, one filter.'); init.v = {}; init.kind = 'Init.init'
    for rec in RECIPES:
        n = rec['n']
        pick = set(R2.sample(range(n), min(n, (3 if n < 16 else 4) if rec['new'] else 2)))
        for i in range(n):
            attempt = 0
            while True:
                R.seed(zlib.crc32(('%s|%s|%d|%d' % (rec['cat'], rec['key'], i, attempt)).encode()))
                p = Patch('?', rec['cat'], '', '')
                p.name = name_for(rec['nouns'])
                rec['fn'](p)
                global_flavours(p, rec['cat'])
                macros_std(p)
                p.tags = finish_tags(p, rec['tags'])
                p.desc = describe(rec['desc'], p.tags, p)
                p.kind = rec['cat'] + '.' + rec['key']
                if p.name in REJECT and attempt < 30:
                    attempt += 1
                    continue
                break
            seeds.append(p)
            if i in pick: factory.append(p)
    order = ['Init', 'Pluck', 'Lead', 'Bell', 'Keys', 'Pad', 'Arp', 'Bass', 'Stab', 'Orchestral', 'Vox', 'Motion', 'FX']
    factory = [init] + sorted(factory, key=lambda q: (order.index(q.cat), q.name))
    return seeds, factory

import random as _random
R2 = _random.Random(7)

if __name__ == '__main__':
    seeds, factory = build_bank()
    hdr = '# GENERATED by tools/gen_presets.py - name, category, tags, description, values, kind\n'
    with open(os.path.join(X.root, 'Resources', 'FactoryPresets.txt'), 'w') as f:
        f.write(hdr)
        for p in factory: f.write(p.line(GAINS) + '\t' + p.kind + '\n')
    with open(os.path.join(X.root, 'Resources', 'SeedPatches.txt'), 'w') as f:
        f.write(hdr)
        for p in seeds: f.write(p.line(GAINS) + '\t' + p.kind + '\n')
    cats = {}
    for p in factory: cats[p.cat] = cats.get(p.cat, 0) + 1
    print(len(factory), 'factory presets', cats, '|', len(seeds), 'seeds,', len(RECIPES), 'recipes')
    gen_tools.write_banks()
