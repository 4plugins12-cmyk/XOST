#!/usr/bin/env python3
"""Factory banks for XO's Matrix (gate / filter / pan / time shaper) and Minirack (modulation rack).
Writes Resources/MatrixPresets.txt and Resources/RackPresets.txt. gen_presets.py also borrows these as 'flavours'."""
import random, os
import xo_lib as X
from xo_lib import lane_gate, lane_steps, lane_pump, lane_sine, lane_ramp, lane_random, Patch

def matrix_bank():
    rnd = random.Random(77)
    out = []
    def add(name, cat, desc, lanes, **kw):
        p = Patch(name, cat, '', desc); p.v = {}
        p.matrix(lanes, **kw); out.append(p)
    # ---- gates
    add('Trance Gate 1/16', 'Gate', 'Classic 16th chop.', {0: lane_gate(16, 0.55)})
    add('Trance Gate 1/8', 'Gate', 'Eighth-note chop.', {0: lane_gate(8, 0.5)})
    add('Gate 3-3-2', 'Gate', 'Dotted trap rhythm.', {0: lane_steps([1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0])})
    add('Gate Triplets', 'Gate', '1/8 triplet chop.', {0: lane_gate(12, 0.5)})
    add('Gate Skippy', 'Gate', 'Skipping 16ths with gaps.', {0: lane_gate(16, 0.6, [1, 0, 1, 1, 0, 1, 1, 0])})
    add('Gate Stutter End', 'Gate', 'Opens up, stutters at the end of the bar.', {0: lane_steps([1] * 12 + [1, 0, 1, 0]) })
    add('Gate Half Bar', 'Gate', 'Every other beat cut.', {0: lane_gate(4, 0.5)})
    add('Gate 1/32 Roll', 'Gate', 'Fast 32nd roll like a hi-hat.', {0: lane_gate(32, 0.5)})
    add('Gate Soft 1/16', 'Gate', 'Gentle 16th pulse (doesn\'t close fully).', {0: lane_gate(16, 0.6, low=0.35)})
    add('Gate Morse', 'Gate', 'Irregular long-short pattern.', {0: lane_steps([1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1, 0])})
    add('Gate Rage', 'Gate', 'Aggressive 16th chop with accents.', {0: lane_gate(16, 0.4, [1, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1])})
    add('Gate 2 Bar Build', 'Gate', 'Gate gets denser over 2 bars.', {0: lane_steps([1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1])}, lens={0: '2bar'})
    add('Gate Dembow', 'Gate', 'Syncopated reggaeton-style gate.', {0: lane_steps([1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0])})
    add('Gate Offbeat', 'Gate', 'Only the off-beats.', {0: lane_steps([0, 0, 1, 1] * 4)})
    add('Gate Swing', 'Gate', 'Swung 16ths.', {0: [(0, 1, 0), (0.06, 1, 0), (0.062, 0, 0), (0.09, 0, 0), (0.092, 1, 0), (0.12, 1, 0), (0.122, 0, 0)] * 1 + [(x + 0.125 * k, y, c) for k in range(1, 8) for x, y, c in [(0, 1, 0), (0.06, 1, 0), (0.062, 0, 0), (0.09, 0, 0), (0.092, 1, 0), (0.12, 1, 0), (0.122, 0, 0)]]})
    add('Gate Random', 'Gate', 'A random-looking but repeating chop.', {0: lane_steps([1 if rnd.random() < 0.65 else 0 for _ in range(16)])})
    # ---- sidechain
    add('Sidechain 1/4', 'Sidechain', 'Four-on-the-floor pump.', {0: lane_pump(4)})
    add('Sidechain 1/8', 'Sidechain', 'Fast eighth pump.', {0: lane_pump(8)})
    add('Sidechain Soft', 'Sidechain', 'Subtle breathing pump.', {0: lane_pump(4, 0.55)})
    add('Sidechain Trap', 'Sidechain', 'Pumps on a trap kick pattern.', {0: [(0, 0, 0), (0.03, 0.1, -0.6), (0.2, 1, 0), (0.375, 0, 0), (0.4, 0.1, -0.6), (0.55, 1, 0), (0.625, 0, 0), (0.65, 0.1, -0.6), (0.8, 1, 0)]})
    add('Sidechain Half-Time', 'Sidechain', 'Big slow half-time duck.', {0: lane_pump(2)})
    add('Sidechain Hard', 'Sidechain', 'Deep, snappy duck.', {0: lane_pump(4, 1.0, -0.9)})
    add('Duck + Filter', 'Sidechain', 'Pump with the filter opening each beat.', {0: lane_pump(4), 1: lane_pump(4, 0.5)})
    add('Sidechain 1/16 Wobble', 'Sidechain', 'Fluttery 16th duck.', {0: lane_pump(16, 0.6)})
    # ---- filter
    add('Filter Sweep Up', 'Filter', 'Low-pass opens over a bar.', {1: lane_ramp(True, 0.4, 0.15, 1.0)})
    add('Filter Sweep Down', 'Filter', 'Low-pass closes over a bar.', {1: lane_ramp(False, -0.4, 0.15, 1.0)})
    add('Filter 2 Bar Riser', 'Filter', 'Slow two-bar build.', {1: lane_ramp(True, 0.5, 0.1, 1.0)}, lens={1: '2bar'})
    add('Filter Wobble 1/8', 'Filter', 'Dubstep-style wobble.', {1: lane_sine(8, 0.25, 0.9, 64)}, flt_reso=0.55)
    add('Filter Wobble 1/4T', 'Filter', 'Triplet wobble.', {1: lane_sine(6, 0.2, 0.85, 48)}, flt_reso=0.5)
    add('Filter Steps', 'Filter', 'Stepped filter melody.', {1: lane_steps([0.4, 0.7, 0.5, 0.9, 0.3, 0.6, 0.8, 0.45])}, flt_reso=0.45)
    add('Filter Pluck', 'Filter', 'Every 8th note gets a filter pluck.', {1: [(k / 8, 0.95, 0.0) for k in range(8)] + [(k / 8 + 0.09, 0.35, 0.6) for k in range(8)]}, flt_reso=0.4)
    add('Telephone', 'Filter', 'Band-pass radio voice that moves.', {1: lane_sine(1, 0.5, 0.7)}, flt_type=2, flt_reso=0.5)
    add('High-Pass Build', 'Filter', 'Thins out over 2 bars for a drop.', {1: lane_ramp(True, 0.6, 0.0, 0.55)}, flt_type=1, lens={1: '2bar'})
    add('High-Pass Chop', 'Filter', 'Low end cut on the off-beats.', {1: lane_steps([0, 0, 0.5, 0.5] * 4)}, flt_type=1)
    add('Underwater', 'Filter', 'Dark muffled drift.', {1: lane_sine(1, 0.2, 0.4)}, flt_reso=0.2)
    add('Filter Random 1/16', 'Filter', 'Random 16th filter steps.', {1: lane_random(16, 0.3, 0.95, rnd)}, flt_reso=0.4)
    # ---- pan
    add('Auto Pan 1/4', 'Pan', 'Smooth quarter-note pan.', {2: lane_sine(2, 0.1, 0.9)})
    add('Auto Pan 1/8', 'Pan', 'Faster pan.', {2: lane_sine(4, 0.15, 0.85)})
    add('Ping Pong 1/8', 'Pan', 'Hard left-right eighths.', {2: lane_steps([0.05, 0.95] * 4)})
    add('Ping Pong 1/16', 'Pan', 'Hard left-right sixteenths.', {2: lane_steps([0.1, 0.9] * 8)})
    add('Pan Drift', 'Pan', 'Slow stereo drift over 4 bars.', {2: lane_sine(1, 0.25, 0.75)}, lens={2: '4bar'})
    add('Pan Random', 'Pan', 'Random 16th positions.', {2: lane_random(16, 0.1, 0.9, rnd)})
    add('Pan Circle', 'Pan', 'Swirls around once per bar.', {2: lane_sine(1, 0.0, 1.0, 32)})
    add('Pan Triplets', 'Pan', 'Triplet left-centre-right.', {2: lane_steps([0.1, 0.5, 0.9] * 4)})
    # ---- time (Gross Beat style)
    add('Half Speed', 'Time', 'Half-time slowdown on every bar.', {3: lane_ramp(True, 0.0, 0.0, 1.0)}, time_range=4)
    add('Tape Stop', 'Time', 'Tape stop at the end of the bar.', {3: [(0, 0, 0), (0.5, 0, 0.7), (0.995, 1, 0)]}, time_range=3)
    add('Stutter Last Beat', 'Time', 'Repeats the last beat in 16ths.', {3: lane_steps([0] * 12 + [0, 0.25, 0.5, 0.75])}, time_range=2)
    add('Stutter 1/8 Repeat', 'Time', 'Eighth repeats on beat 4.', {3: lane_steps([0] * 6 + [0, 0.5])}, time_range=2)
    add('Backspin', 'Time', 'Fake rewind at the end of the bar.', {3: [(0, 0, 0), (0.75, 0, 0), (0.995, 1, -0.6)]}, time_range=4)
    add('Repeat 1/16', 'Time', 'Machine-gun 16th repeats.', {3: [(k / 16, 0.0, 0.0) for k in range(16)] + [(k / 16 + 0.06, 1.0, 0.0) for k in range(16)]}, time_range=0)
    add('Glitch Time', 'Time', 'Random jumps backwards in time.', {3: lane_random(16, 0.0, 0.8, rnd)}, time_range=2)
    add('Wobble Speed', 'Time', 'Pitch wobbles like a warped record.', {3: lane_sine(2, 0.0, 0.3, 32)}, time_range=0)
    add('Triplet Repeat', 'Time', 'Repeats in triplets.', {3: lane_steps([0, 0.33, 0.66] * 4)}, time_range=1)
    add('Echo Gate', 'Time', 'Gated with half-beat echoes.', {0: lane_gate(8, 0.6), 3: lane_steps([0, 0.5] * 4)}, time_range=1)
    add('Slowdown 2 Bars', 'Time', 'Gradual slowdown over two bars.', {3: lane_ramp(True, 0.3, 0.0, 1.0)}, lens={3: '2bar'}, time_range=4)
    add('Reverse-ish', 'Time', 'Plays the last half beat backwards-ish.', {3: [(k / 8, 0.0, 0.0) for k in range(8)] + [(k / 8 + 0.12, 0.95, 0.0) for k in range(8)]}, time_range=1)
    # ---- combos
    add('Gate + Pan', 'Combo', 'Chopped and bouncing left-right.', {0: lane_gate(16, 0.55), 2: lane_steps([0.2, 0.8] * 4)})
    add('Pump + Sweep', 'Combo', 'Sidechain pump and a filter build.', {0: lane_pump(4), 1: lane_ramp(True, 0.4, 0.3, 1.0)})
    add('Rage Chop', 'Combo', 'Hard gate, filter stabs and pans.', {0: lane_gate(16, 0.45, [1, 1, 0, 1]), 1: lane_steps([0.9, 0.5] * 8), 2: lane_steps([0.3, 0.7] * 8)})
    add('Hyper Glitch', 'Combo', 'Gate, time jumps and random pan.', {0: lane_gate(16, 0.6, [1, 1, 1, 0]), 3: lane_random(16, 0.0, 0.5, rnd), 2: lane_random(8, 0.2, 0.8, rnd)}, time_range=1)
    add('Dream Wash', 'Combo', 'Slow filter and pan drift.', {1: lane_sine(1, 0.4, 0.9), 2: lane_sine(1, 0.3, 0.7)}, lens={1: '2bar', 2: '4bar'})
    add('Lofi Wobble', 'Combo', 'Pitch wobble and muffled filter.', {1: lane_sine(1, 0.45, 0.6), 3: lane_sine(1, 0.0, 0.2, 32)}, time_range=0)
    add('Stutter Drop', 'Combo', 'Gate, then a tape stop on the last beat.', {0: lane_gate(16, 0.55), 3: [(0, 0, 0), (0.75, 0, 0.6), (0.995, 1, 0)]}, time_range=2)
    add('Triplet Bounce', 'Combo', 'Triplet gate + pan.', {0: lane_gate(12, 0.5), 2: lane_steps([0.2, 0.5, 0.8] * 4)})
    add('Breathing', 'Combo', 'Soft duck + filter breath.', {0: lane_pump(2, 0.5), 1: lane_sine(2, 0.5, 0.95)})
    add('Chaos Desk', 'Combo', 'Everything moving at once.', {0: lane_random(16, 0.3, 1.0, rnd), 1: lane_random(8, 0.3, 0.9, rnd), 2: lane_random(16, 0.1, 0.9, rnd), 3: lane_random(16, 0.0, 0.3, rnd)}, time_range=1)
    return out

def rack_bank():
    out = []
    def add(name, cat, desc, modules, depth=1.0):
        p = Patch(name, cat, '', desc); p.v = {}
        p.rack(modules, depth); out.append(p)
    add('Slow Breathing', 'Movement', 'Slow filter + wavetable breathing.', [dict(type='lfo', div='2bar', a=0.0, dst='cut1', amt=0.25, dst2='apos', amt2=0.2)])
    add('Wavetable Scan', 'Movement', 'Scans oscillator A every bar.', [dict(type='lfo', div='1bar', a=0.33, dst='apos', amt=0.45)])
    add('Pulse Filter 1/8', 'Movement', 'Eighth-note filter pulse.', [dict(type='lfo', div='1/8', a=0.66, dst='cut1', amt=0.35)])
    add('Wide Stereo Motion', 'Movement', 'Voices drift across the stereo field.', [dict(type='lfo', div='1/2', a=0.0, dst='pan', amt=0.5), dict(type='lfo', div='1bar', a=0.33, b=0.6, dst='apan', amt=0.4)])
    add('Tremolo', 'Movement', 'Classic volume tremolo.', [dict(type='lfo', div='1/16', a=0.0, dst='amp', amt=-0.45)])
    add('Vibrato Deep', 'Movement', 'Wide singer-style vibrato.', [dict(type='lfo', sync=0, rate=5.2, a=0.0, dst='apitch', amt=0.01, dst2='bpitch', amt2=0.01)])
    add('Bounce Filter', 'Movement', 'Bouncing-ball filter each beat.', [dict(type='bounce', div='1/2', a=0.5, b=0.4, dst='cut1', amt=0.4)])
    add('Bounce Volume', 'Movement', 'Each bar bounces like a ball.', [dict(type='bounce', div='1bar', a=0.7, b=0.5, dst='amp', amt=0.5)])
    add('Random Filter Steps', 'Random', 'Sample & hold on the cutoff every 1/16.', [dict(type='sh', div='1/16', a=1.0, b=0.0, dst='cut1', amt=0.35)])
    add('Random Glide Filter', 'Random', 'Smooth random filter.', [dict(type='sh', div='1/8', a=0.8, b=0.6, dst='cut1', amt=0.35)])
    add('Drunk Pitch', 'Random', 'Tiny drunken pitch wander.', [dict(type='drunk', div='1/8', a=0.3, b=0.4, dst='apitch', amt=0.008, dst2='bpitch', amt2=-0.008)])
    add('Drunk Wavetable', 'Random', 'Wavetable position wanders.', [dict(type='drunk', div='1/16', a=0.4, b=0.2, dst='apos', amt=0.4)])
    add('Random Pan Notes', 'Random', 'Every step lands somewhere new.', [dict(type='sh', div='1/16', a=1.0, b=0.0, dst='pan', amt=0.7)])
    add('Random Octave Jumps', 'Random', 'Occasional octave jumps (S&H on pitch).', [dict(type='sh', div='1/8', a=0.25, b=0.0, dst='bpitch', amt=0.5)])
    add('Turing Melody', 'Rhythmic', 'Locked Turing loop on oscillator B pitch.', [dict(type='turing', div='1/16', a=0.45, b=0.0, dst='bpitch', amt=0.25)])
    add('Turing Filter', 'Rhythmic', 'Turing loop on the filter.', [dict(type='turing', div='1/16', a=0.5, b=0.1, dst='cut1', amt=0.35)])
    add('Turing Mutating', 'Rhythmic', 'A loop that slowly mutates.', [dict(type='turing', div='1/8', a=0.6, b=0.35, dst='apos', amt=0.5, dst2='cut1', amt2=0.2)])
    add('Step Seq Filter', 'Rhythmic', '8-step filter sequence.', [dict(type='seq', div='1bar', a=0.31, b=0.0, dst='cut1', amt=0.4)])
    add('Step Seq Gate', 'Rhythmic', '8-step volume pattern.', [dict(type='seq', div='1/2', a=0.72, b=0.0, dst='amp', amt=0.6)])
    add('Step Seq Wavetable', 'Rhythmic', 'Wavetable sequence.', [dict(type='seq', div='1bar', a=0.13, b=0.1, dst='apos', amt=0.6)])
    add('Chaos Filter', 'Chaos', 'Lorenz attractor on the cutoff.', [dict(type='chaos', sync=0, rate=1.0, a=0.0, b=0.4, dst='cut1', amt=0.35)])
    add('Chaos Pan + Pos', 'Chaos', 'Chaotic pan and wavetable.', [dict(type='chaos', sync=0, rate=0.6, a=0.0, b=0.5, dst='pan', amt=0.6, dst2='apos', amt2=0.3)])
    add('Chaos Everything', 'Chaos', 'Three attractors fighting.', [dict(type='chaos', sync=0, rate=0.8, a=0.0, dst='cut1', amt=0.3), dict(type='chaos', sync=0, rate=1.6, a=1.0, dst='apos', amt=0.4), dict(type='chaos', sync=0, rate=0.4, a=0.0, b=0.8, dst='pan', amt=0.5)])
    add('Follower Bloom', 'Follower', 'Filter opens as the synth gets louder.', [dict(type='follower', a=0.3, b=0.4, dst='cut1', amt=0.4)])
    add('Follower Duck Reverb', 'Follower', 'Reverb ducks while you play, blooms after.', [dict(type='follower', a=0.4, b=0.6, dst='revmix', amt=-0.3)])
    add('Follower Grit', 'Follower', 'Plays harder, gets dirtier.', [dict(type='follower', a=0.2, b=0.5, dst='distdrive', amt=0.5, dst2='distmix', amt2=0.4)])
    add('Analog Drift', 'Subtle', 'Very slow random detune and filter drift.', [dict(type='drunk', sync=0, rate=0.6, a=0.2, b=0.5, dst='adetune', amt=0.1, dst2='cut1', amt2=0.08)])
    add('Living Pad', 'Subtle', 'Slow wavetable, pan and filter motion for pads.', [dict(type='lfo', div='4bar', a=0.0, dst='apos', amt=0.3), dict(type='lfo', div='2bar', a=0.33, dst='pan', amt=0.25), dict(type='drunk', sync=0, rate=0.3, a=0.3, dst='cut1', amt=0.12)])
    add('Shimmer Motion', 'Subtle', 'Gentle chorus and delay movement.', [dict(type='lfo', div='2bar', dst='chorusmix', amt=0.3), dict(type='lfo', div='1bar', a=0.33, dst='delaymix', amt=0.15)])
    add('Tape Warble', 'Subtle', 'Wobbly pitch like a warped cassette.', [dict(type='lfo', sync=0, rate=0.7, a=0.0, dst='apitch', amt=0.006, dst2='bpitch', amt2=0.006), dict(type='drunk', sync=0, rate=3.0, a=0.3, dst='apitch', amt=0.003)])
    add('Rage Siren', 'Extreme', 'Big pitch siren sweeps.', [dict(type='lfo', div='1bar', a=0.33, dst='apitch', amt=0.25)])
    add('Glitch Storm', 'Extreme', 'S&H on everything, fast.', [dict(type='sh', div='1/32', a=1.0, dst='apos', amt=0.6, dst2='cut1', amt2=0.4), dict(type='sh', div='1/16', a=0.7, dst='pan', amt=0.8)])
    add('FM Madness', 'Extreme', 'Oscillator mod amount gets thrown around.', [dict(type='turing', div='1/16', a=0.3, b=0.2, dst='amod', amt=0.6)])
    add('Bit Chaos', 'Extreme', 'Distortion drive chaos.', [dict(type='chaos', sync=0, rate=2.0, dst='distdrive', amt=0.6), dict(type='sh', div='1/8', dst='shpdrive', amt=0.5)])
    add('Laser Bounce', 'Extreme', 'Bouncing pitch drops every beat.', [dict(type='bounce', div='1/4', a=0.6, b=0.3, dst='apitch', amt=0.3)])
    add('Wobble Bass', 'Extreme', 'Synced wobble on cutoff and drive.', [dict(type='lfo', div='1/8T', a=0.0, dst='cut1', amt=0.55, dst2='drive1', amt2=0.4)])
    add('Rhythmic Pan Seq', 'Rhythmic', 'Sequenced pan positions.', [dict(type='seq', div='1/2', a=0.55, dst='pan', amt=0.8)])
    add('Pitch Seq Fifths', 'Rhythmic', 'Oscillator B jumps between intervals.', [dict(type='seq', div='1bar', a=0.88, dst='bpitch', amt=0.3)])
    add('Two Speed LFO', 'Movement', 'Fast and slow LFOs combined.', [dict(type='lfo', div='1/16', a=0.0, dst='cut1', amt=0.15), dict(type='lfo', div='2bar', a=0.0, dst='cut1', amt=0.3)])
    add('Unison Breathing', 'Subtle', 'Detune opens and closes.', [dict(type='lfo', div='2bar', a=0.0, dst='adetune', amt=0.3, dst2='bdetune', amt2=0.3)])
    return out

def write_banks():
    for fname, bank in (('MatrixPresets.txt', matrix_bank()), ('RackPresets.txt', rack_bank())):
        with open(os.path.join(X.root, 'Resources', fname), 'w') as f:
            f.write('# GENERATED by tools/gen_tools.py\n')
            for p in bank: f.write(p.line({}) + '\n')
        print(fname, len(bank))

if __name__ == '__main__':
    write_banks()
