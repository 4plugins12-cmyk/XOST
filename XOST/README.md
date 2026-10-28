# XOST — Text-to-Sound Synthesizer

XOST is XO rebuilt around one idea: **type what you want to hear, and the synth builds it.** You don't need to load a
preset first.

Type something like `dark glassy pluck like pierre bourne with tape`, press **GENERATE**, and XOST makes four
different sounds that match. Then you can:

- keep tweaking the one you like with words such as `darker`, `more reverb`, `no chorus` or `wider`;
- export it as a trimmed one-shot WAV with one click.

It runs as VST3 everywhere, and also as AU on macOS. It keeps everything XO has: 3 wavetable oscillators, the sampler,
2 filters, 3 envelopes, 4 LFOs, the 16-slot mod matrix, the arp, Chord Memory, 9 effects, **MATRIX**, **MINIRACK**,
**MUTATE**, undo, bounce and resample.

The whole look is XO's black layout with a Zenology-style **light blue** in place of the white.

**It is free and works offline.** There is no AI, no internet and no subscription: everything happens inside the plugin.

---

## Getting the plugin file

Same GitHub method as before, with a **new repository** (for example `xost`):

1. Upload everything from this folder: `CMakeLists.txt`, `README.md`, and the `Source`, `Resources`, `Tests` and `tools`
   folders. **`Resources` holds the fonts, the presets, the sound library and the word list, so don't skip it.**
2. If the `.github` folder didn't upload, go to **Actions → set up a workflow yourself**, paste in
   `.github/workflows/build.yml`, and commit.
3. When the run shows a green tick, download **XOST-Windows** or **XOST-macOS** from *Artifacts*.

Install `XOST.vst3` to:
- Windows: `C:\Program Files\Common Files\VST3\`
- Mac: `~/Library/Audio/Plug-Ins/VST3/` (the AU `XOST.component` goes in `~/Library/Audio/Plug-Ins/Components/`)

On a Mac, run this once:

```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/XOST.vst3 ~/Library/Audio/Plug-Ins/Components/XOST.component
```

In FL Studio, open *Options → Manage plugins → Find plugins*, then load XOST as a **Generator**.

---

## Making sounds with words

The **prompt bar** runs across the top. Type a description, then press **Enter** or **GENERATE**.

- **Chips** appear under the box as you type, showing what XOST understood. Each colour is a kind of word:
  - **sound** (blue): `pluck`, `808`, `flute`
  - **style** (purple): `PIERRE BOURNE STYLE`
  - **character** (green): `dark`, `glassy`
  - **effect** (amber): `tape`, `sidechain`

  Crossed-out grey chips are words it doesn't know, which it skips. A chip ending in `?` is a typo it fixed for you
  (`cowbel` → `COWBELL?`).
- **Four variations (A–D)** appear in the CREATE column on the left. Click a card to load it; click its ▶ to load it and
  hear a note.
  - **GENERATE AGAIN** makes four new sounds from the same words.
  - **MORE LIKE THIS** makes four new sounds from the same family as the selected one.
- **SURPRISE** writes a random description and generates it. It's a good way to discover words. **?** shows example
  prompts.
- **HISTORY** keeps every sound you've generated, together with the words you used. Click one to bring it back.
  Right-click for more options: put the words back in the box, remove it, or clear the list.
  - The history and variations are saved with your project.
  - Use SAVE in the header to keep a sound as a normal preset.

### Tweaking the current sound

Turn on **TWEAK** and type words that change the sound you already have, such as `darker`, `more reverb`,
`less wobble`, `no chorus`, `a lot wider` or `add tape`.

You often don't need the button. Words like `more`, `less`, `no` or `darker` switch to tweaking automatically when the box
has no sound or style word in it.

Twelve **QUICK TWEAKS** buttons do the common moves in one click: DARKER, BRIGHTER, WIDER, SOFTER, PUNCHIER,
GRITTIER, MORE SPACE, DRIER, LO-FI, MOVEMENT, SHORTER and LONGER. Every change can be undone.

### Words it knows (about 250 entries, each with several spellings)

| Kind | Examples |
|---|---|
| **Sounds** | pluck, lead, bell, keys, pad, arp, bass, stab, strings, vox, fx · 808, slide 808, distorted 808, sub, reese, piano, rhodes / ep, organ, harpsichord, guitar, harp, koto, flute, whistle, sine, square, supersaw, choir, brass, pizzicato, music box, glock, celesta, cowbell, church bell, vocal chop, talkbox, siren, riser, impact, zap, drone, texture, chiptune... |
| **Styles** | trap, dark trap, rage, hyperpop, ambient, plugg, dark plugg, opium, Carti, Whole Lotta Red, vamp, Ken Carson, Destroy Lonely, Osamason, Che, Future, Lil Uzi Vert, bleood, Jetsonmade, TM88, Outtatown, StarBoy, Richie Souf, Pierre Bourne, Clayco, Loesoe, Ginseng, Brandon Finessin, Thrty / Boolymon / Margiela / Mexikodro, **plus the new scenes below** |
| **Character** | dark, bright, warm, cold, glassy, metallic, soft, hard, gritty, clean, airy, hollow, wide, narrow, detuned, fat, thin, lofi, retro, analog, dreamy, haunted, sad, happy, evil, bouncy, punchy, short, long, swell, smooth, crunchy, fuzzy, wet, dry, deep / low, high, underwater, distant, spacey, cinematic, glitchy, chaotic, wonky, organic, robotic, sparkly, round, sharp, chill, heavy, moving, jazzy... |
| **Effects** | reverb, huge reverb, delay, chorus, phaser, flanger, distortion, bitcrush, tape, saturation, compression, sidechain, gate, stutter, wobble, vibrato, tremolo, autopan, filter sweep, glide / slide, pitch drop, noise, sub layer, chords, minor / major / 7th / 9th / power chord, strum, arpeggiated, mono, poly, unison |
| **Amounts** | a little, slightly, kinda · very, super, hella · extremely, max · no, without · more, less |

The whole word list is plain text in `Resources/Lexicon.txt`, so you can add your own words and rebuild.

### How it works (and its limits)

XOST matches **keywords**; it doesn't understand full sentences. `a dark pluck that isn't too bright` works because it
sees *dark* and *pluck*, but grammar is ignored, so short descriptions work best.

1. The sound and style words choose a **sound family**. These families come from a hidden library of **about 2,000
   synth recipes** (125 families).
2. XOST scores every recipe against your words and picks four different matches. To make each one new, it sometimes
   crosses the match with a sibling from the same family and adds a small random variation.
3. Every **character** and **effect** word then moves real parameters. For example, `dark` lowers the filters and the
   top end, `tape` turns on and sets the Tape effect, and `sidechain` loads a pumping Matrix pattern. `very` and
   `a little` scale how far each move goes.
4. Each result is rendered offline and **loudness-matched**, so the four variations, your history and the factory
   presets all sit at the same level.

Generating four sounds takes a second or two. It runs on a background thread, so the audio never stops.

---

## One-shot export

The **ONE-SHOT** box is at the top right. Press **EXPORT ONE-SHOT** to render the current sound as a single note, which is
then:

- trimmed: no silence at the front, and the tail is cut where it dies away, with tiny fades so it never clicks;
- normalised to −0.3 dB;
- named with its **real root note**. XOST checks the pitch it actually rendered, so a patch with its oscillator an
  octave up is named one octave higher. For example: `Dark Glassy Drop (C5).wav`.

**Drag** the chip under the button straight into FL's playlist, a sampler or Uno. Files are also kept in
`Documents/XOST/One Shots` (FOLDER opens it).

**OPTIONS** sets:
- **the note:** Auto, which is middle C, or low C for bass;
- **the length:** Auto (short for plucks and hits, longer for leads, longest for pads), or short / 1 s / 2.5 s;
- **the note-name style:** FL Studio C5, Logic C4 or Ableton C3;
- **normalise:** on or off.

---

## Presets

There are **308 factory presets**, about 2–4 from each family, all loudness-matched. They're on the **PRESETS** tab of
the left column and in **BROWSE**. Matrix (66) and Minirack (40) presets are carried over from XO.

The browser has **genre** filters:
- Trap, Rage, Hyperpop, Ambient, Plugg, Pluggnb, Dark, Bouncy, Melodic
- Phonk, Jerk, Drill, Detroit, Jersey Club, Cloud, Digicore, Sigilkore, Emo, RnB

It also has **type-beat** filters: XO's artists plus Yeat, F1lthy, Summrs, xaviersobased, Luci4, glaive, Drain Gang,
Clams Casino, Kordhell, Cash Cobain, Metro Boomin, Lil Peep, Sematary and SOPHIE.

Your own presets save as `.xostpreset` in `Documents/XOST/Presets`.

---

## Research: the new styles

I searched the web for scenes and producers close to your list, then turned what I found into new sound recipes and
vocabulary. The descriptions below are my interpretation of each scene; none of them are the producers' actual sounds or
samples.

| Style words | What XOST builds | Based on |
|---|---|---|
| **yeat, bnyx, synthetic** | sharp, echoing tolling bells and chime leads | Wikipedia on rage and on Yeat's "The Bell" (produced by Synthetic and others): "sharp, echoing chimes" over heavy bass; Yeat popularised bells in rage |
| **f1lthy, working on dying** | crushed, warbly "mp3" supersaw leads | F1lthy produced most of *Whole Lotta Red*; rage = stereo-wide EDM/trance-like leads, overdriven 808s |
| **pluggnb, summrs, autumn!, xangang, arizona tears** | EP block chords (min9 / add9), pure sine leads with glide, glassy bells | Splice: block chords on electric piano with 7ths/9ths, sine-wave leads, glassy bells |
| **jerk, xaviersobased, nettspend, kashpaint** | twitchy degraded leads with pitch zaps, ducking bass, FX-spam | Wikipedia: fidgety rhythms, sidechained bass, sequences of degraded sound effects ("FXspam") |
| **phonk, drift phonk, kordhell, memphis** | melodic crushed 808-style cowbell, blown-out 808, dusty Memphis keys | eMastered / LANDR: melodic cowbell leads, blown-out distorted bass, lo-fi bit-crushed cowbell, sinister minor keys |
| **sigilkore, luci4, islurwhenitalk** | bitcrushed, pitch-shifted ambient pads and leads | Wikipedia: bitcrushing and pitch shifting, trap drums with an ambient atmosphere |
| **digicore, glitchcore, glaive, ericdoa** | bright sparkly plucks, hard-tuned vocal-ish leads | Wikipedia: layered pluggnb-style melodies, Auto-Tune as an instrument, sharp 808s |
| **cloud rap, clams casino, drain gang, bladee, whitearmor** | reverb-drenched pitched vocal pads, soft trance supersaw pads, fluid delayed plucks | Splice: chopped/pitched samples "drenched in reverb", trance supersaws; Wikipedia: Whitearmor's "softly textured and fluid" sound |
| **jersey club** | chopped vocal stabs, the "bed squeak" | Orphiq: melody from very short chopped vocal stabs, the bed-squeak sound |
| **uk / ny drill, m1onthebeat, chicago drill / glo** | 808s with long slides, halftime dark piano, brassy horn stabs | Attack Magazine: gliding 808s, halftime piano melodies |
| **detroit, helluva, tee grizzley, 42 dugg** | dark minor piano with a lo-fi edge, sliding 808s | Output: dark minor piano riffs with reverb or lo-fi texture, sliding 808s, 140–160 BPM |
| **sexy drill, cash cobain** | smooth R&B keys with chorus and tape, soul-sample chops | Revolt: drill built on classic R&B samples, smooth and laid-back |
| **metro boomin, southside, wheezy** | haunting bells, cinematic dark piano, tremolo ghost strings, flute | Melodics: moody minor, cinematic piano, ghostly strings, bell melodies, the "Mask Off" flute |
| **emo rap, lil peep, juice wrld** | clean sad guitar plucks and strums, fuzzy guitar leads | Wikipedia: trap beats with indie-rock / emo guitar instrumentals |
| **sematary, haunted mound** | blown-out, detuned horror leads | Pumpehuset: Sematary as a "horror rapper" with his Haunted Mound collective |
| **hyperpop, sophie, a. g. cook** | glossy, rubbery, metallic plucks and squelchy basses | Wikipedia: metallic, melodic percussion, brash synths, heavy compression |

Sources:
- [Rage (music genre) – Wikipedia](https://en.wikipedia.org/wiki/Rage_(music_genre))
- [The Bell (Yeat song) – Wikipedia](https://en.wikipedia.org/wiki/The_Bell_(Yeat_song))
- [Pluggnb: what it is – Splice](https://splice.com/blog/what-is-pluggnb/)
- [Jerk (music genre) – Wikipedia](https://en.wikipedia.org/wiki/Jerk_(music_genre))
- [What is Drift Phonk? – eMastered](https://emastered.com/blog/what-is-drift-phonk)
- [What is Phonk? – LANDR](https://blog.landr.com/what-is-phonk)
- [Sigilkore – Wikipedia](https://en.wikipedia.org/wiki/Sigilkore)
- [Digicore – Wikiwand](https://www.wikiwand.com/en/articles/Digicore)
- [What is cloud rap? – Splice](https://splice.com/blog/what-is-cloud-rap/)
- [Whitearmor – Wikipedia](https://en.wikipedia.org/wiki/Whitearmor)
- [What is Jersey club? – Orphiq](https://orphiq.com/resources/what-is-jersey-club)
- [UK drill: beat dissected – Attack Magazine](https://www.attackmagazine.com/technique/beat-dissected/uk-drill/)
- [Detroit type beats explained – Output](https://output.com/blog/detroit-type-beat)
- [Explaining sexy drill – Revolt](https://www.revolt.tv/article/explaining-sexy-drill-and-some-of-the-subgenre-top-artists)
- [Metro Boomin producer deep dive – Melodics](https://melodics.com/blog/metro-boomin-producer-deep-dive)
- [Emo rap – Wikipedia](https://en.wikipedia.org/wiki/Emo_rap)
- [Sematary and Haunted Mound – Pumpehuset](https://pumpehuset.dk/en/?p=63609)
- [Hyperpop – Wikipedia](https://en.wikipedia.org/wiki/Hyperpop)
- [Best synth plugins for rage and Opium beats – Internet Tattoo](https://www.internettattoo.com/blog/best-synth-plugins-for-rage-and-opium-trap-beats)

---

## Everything from XO

- **OSC / MOD / ROUTING / FX / ARP:** the full synth, the same as XO.
- **MATRIX:** a Gross Beat / ShaperBox-style gate, filter, pan and time shaper.
- **MINIRACK:** six patchable modulators.
- **MUTATE:** the gene field, breeding and morphing.

See XO's README for the details of each page. The header has BROWSE, SAVE, INIT, RANDOM, LOCKS, UNDO / REDO / HISTORY
and BOUNCE. The window is resizable from 50% to 200%.

---

To build it yourself: `cmake -B build -DCMAKE_BUILD_TYPE=Release`, then
`cmake --build build --config Release --target XOST_VST3`.

- `tools/gen_presets.py` generates the sound library (`SeedPatches.txt`) and the factory presets.
- `tools/calibrate.py` loudness-matches the presets.
- `Tests/` holds the test harness: it renders every preset, runs 25 prompts and 10 tweaks through the word engine, and
  checks the one-shot exporter.

Fonts: Space Grotesk and Inter (SIL Open Font License, see `Resources/`). Built with JUCE 8, which is free for
personal projects. Read JUCE's licence before selling the plugin.
