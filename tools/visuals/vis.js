// SPDX-License-Identifier: GPL-3.0-only
// Sornyak's visuals for a projector: the firmware's flora engine (sornyak-visuals.wasm, tools/visuals/vis.c) fed from
// the FM-1's MIDI out, as the firmware feeds it (cf_ui.c cf_feed_plant). From MIDI it knows the notes on the chord,
// bass, layer and drums channels (GLOBAL > Channels on the FM-1: 1, 2, 3, 10 by default) and the clock; from them:
// the chord (an arpeggio's notes inside the chord keep it), the bass, a pulse a note, the loudness, the tempo and the
// place in the bar, which channels the pattern plays (the tiers and the butterflies), and live notes (the layer's).
"use strict";
(() => {
  const $ = (id) => document.getElementById(id);
  const cv = $("cv"), ctx = cv.getContext("2d");
  const status = (s) => { $("status").textContent = s; };
  const SIZES = [[160, 90], [120, 70]];
  const Q = { NONE: 0, MAJOR: 1, MINOR: 2, DIM: 3, AUG: 4, SUS2: 5, SUS4: 6, SINGLE: 7 };
  const FORMS = [[Q.MAJOR, [0, 4, 7]], [Q.MINOR, [0, 3, 7]], [Q.DIM, [0, 3, 6]], [Q.AUG, [0, 4, 8]],
                 [Q.SUS2, [0, 2, 7]], [Q.SUS4, [0, 5, 7]]];
  let W, wasm, mem, img, size = 0, species = 0;
  let intro = null;             // the intro playing: {t0}; the FM-1's own, when it comes on (cf_splash.c)

  // ---- the state the MIDI builds (cf_feed_plant's inputs)
  const st = {
    held: new Map(),            // chord channel: note -> velocity
    chord: [], chordPcs: new Set(), chordAt: 0, pending: null,
    bass: -1, lvl: 0, lastNote: new Map(),   // per role: performance.now() of its last note
    clock: false, ticks: 0, tickTimes: [], bpm: 120, beats: 4, tracks: 0,
  };
  const ch = () => ({ chord: +$("chC").value - 1, bass: +$("chB").value - 1, layer: +$("chL").value - 1,
                      drums: +$("chD").value - 1 });

  // ---- the chord: its root and quality from the notes held
  function classify(notes) {
    const pcs = [...new Set(notes.map((n) => n % 12))];
    if (pcs.length === 1) return { root: pcs[0], q: Q.SINGLE };
    for (const [q, iv] of FORMS)
      for (const r of pcs)
        if (iv.every((i) => pcs.includes((r + i) % 12))) return { root: r, q };
    return { root: Math.min(...notes) % 12, q: pcs.length >= 3 ? Q.MAJOR : Q.SINGLE };
  }
  function setChord(notes) {
    const n = notes.slice().sort((a, b) => a - b).slice(0, 8);
    st.chord = n;
    st.chordPcs = new Set(n.map((x) => x % 12));
    if (!wasm) return;
    const c = n.length ? classify(n) : { root: 0, q: Q.NONE };
    new Uint8Array(mem.buffer, wasm.vis_chord_notes(), 8).set(n.concat([0, 0, 0, 0, 0, 0, 0, 0]).slice(0, 8));
    wasm.vis_chord(n.length, c.root, c.q, st.bass);
    if (n.length) wasm.vis_key(c.root);
  }
  // notes arriving within 40 ms are one chord; a note already in the chord (an arpeggio) keeps it and pulses
  function chordNote(note, vel) {
    if (wasm) wasm.vis_note_on(note, vel);
    if (st.chord.length > 1 && st.chordPcs.has(note % 12) && !st.pending) return;
    clearTimeout(st.pending);
    st.pending = setTimeout(() => {
      st.pending = null;
      const held = [...st.held.keys()];
      if (held.length) setChord(held);
    }, 40);
  }

  // ---- MIDI in
  function onMidi(e) {
    const [s, a = 0, b = 0] = e.data, hi = s & 0xf0, c = s & 15, C = ch(), now = performance.now();
    if (s === 0xf8) {                          // clock: 24 a beat
      st.ticks++;
      st.tickTimes.push(now);
      if (st.tickTimes.length > 48) st.tickTimes.shift();
      if (st.tickTimes.length > 12) {
        const dt = (st.tickTimes[st.tickTimes.length - 1] - st.tickTimes[0]) / (st.tickTimes.length - 1);
        if (dt > 0) st.bpm = Math.round(60000 / (dt * 24));
      }
      return;
    }
    if (s === 0xfa) { st.clock = true; st.ticks = 0; return; }        // start
    if (s === 0xfb) { st.clock = true; return; }                       // continue
    if (s === 0xfc) { st.clock = false; return; }                      // stop
    const on = hi === 0x90 && b > 0, off = hi === 0x80 || (hi === 0x90 && b === 0);
    if (!on && !off) return;
    const role = c === C.chord ? "chord" : c === C.bass ? "bass" : c === C.layer ? "layer" : c === C.drums ? "drums" : "";
    if (!role) return;
    if (on) {
      st.lastNote.set(role, now);
      st.lvl = Math.min(60000, st.lvl + b * (role === "drums" ? 160 : 320));
    }
    if (role === "chord") {
      if (on) { st.held.set(a, b); chordNote(a, b); } else st.held.delete(a);
    } else if (role === "bass") {
      if (on) { st.bass = a; if (wasm) wasm.vis_note_on(a, b); }
    } else if (role === "layer" && on && wasm) {
      wasm.vis_note_on(a, b);
      wasm.vis_live();                         // (live over a pattern: the butterflies come)
    }
  }
  async function midiSetup() {
    if (!navigator.requestMIDIAccess) { status("this browser has no Web MIDI: use Chrome or Edge"); return; }
    let acc;
    try { acc = await navigator.requestMIDIAccess(); } catch { status("MIDI access refused"); return; }
    const sel = $("midi");
    const fill = () => {
      const keep = sel.value;
      sel.length = 1;
      for (const p of acc.inputs.values()) sel.add(new Option(p.name, p.id));
      const fm = [...acc.inputs.values()].find((p) => /felucca|fm-?1|sornyak/i.test(p.name));
      sel.value = [...sel.options].some((o) => o.value === keep) && keep ? keep : fm ? fm.id : "";
      bind();
    };
    // the FM-1 switched on or plugged in: its intro plays here too, with the synth's
    const seen = new Set([...acc.inputs.values()].filter((p) => p.state === "connected").map((p) => p.id));
    acc.addEventListener("statechange", (e) => {
      const p = e.port;
      if (p.type !== "input") return;
      if (p.state === "connected" && !seen.has(p.id) && /felucca|fm-?1|sornyak/i.test(p.name)) playIntro();
      if (p.state === "connected") seen.add(p.id); else seen.delete(p.id);
    });
    const bind = () => {
      for (const p of acc.inputs.values()) p.onmidimessage = p.id === sel.value ? onMidi : null;
      const p = acc.inputs.get(sel.value);
      status(p ? `listening to ${p.name}` : "no MIDI input: plug in the FM-1, or press Demo");
    };
    sel.onchange = bind;
    acc.addEventListener("statechange", fill);
    fill();
  }

  // ---- demo: a pattern of its own, building up a track every two bars (no FM-1 needed)
  let demo = null;
  function demoToggle() {
    if (demo) { clearInterval(demo.t); demo = null; st.clock = false; st.held.clear(); st.tracks = 0; setChord([]);
      $("demo").classList.remove("on"); return; }
    const prog = [[60, 64, 67], [57, 60, 64], [53, 57, 60], [55, 59, 62]];
    let beat = 0;
    demo = { t: setInterval(() => {
      const C = ch(), bar = Math.floor(beat / 4), chord = prog[bar % 4], k = Math.floor(bar / 2) % 6;
      const send = (c, n, v) => onMidi({ data: [0x90 | c, n, v] });
      if (beat === 0) onMidi({ data: [0xfa] });
      for (let i = 0; i < 24; i++) onMidi({ data: [0xf8] });
      if (beat % 4 === 0) { for (const n of st.held.keys()) onMidi({ data: [0x80 | C.chord, n, 0] });
        if (k >= 1) chord.forEach((n) => send(C.chord, n, 96)); }
      if (k >= 2) send(C.bass, chord[0] - 24, 100);
      if (k >= 3 && beat % 2) send(C.layer, chord[beat % 3] + 12, 80);
      if (k >= 4) send(C.drums, 36, 110);
      beat++;
    }, 500) };
    $("demo").classList.add("on");
  }

  // ---- each frame: the state into the engine, a frame drawn
  let last = performance.now();
  function frame(now) {
    const dt = Math.min(100, now - last);
    last = now;
    if (wasm && intro) {                       // the intro: the FM-1's 240 x 240 screen, then the scene again
      if (wasm.vis_intro_frame(Math.round(now - intro.t0))) { drawFb(); requestAnimationFrame(frame); return; }
      intro = null;
      setSize(size);
    }
    if (wasm) {
      const C = st, recent = (role, ms) => now - (C.lastNote.get(role) || -1e9) < ms;
      const beatMs = 60000 / Math.max(30, C.bpm);
      const sustain = C.held.size ? 16000 : 0;
      C.lvl = C.lvl > sustain ? sustain + (C.lvl - sustain) * Math.exp(-dt / 350) : C.lvl;
      if (C.clock) {                           // the tracks the pattern plays: a note in the last two bars
        const span = beatMs * C.beats * 2;
        C.tracks = (recent("chord", span) ? 1 : 0) | (recent("bass", span) ? 2 : 0) |
                   (recent("layer", span) ? 4 : 0) | (recent("drums", span) ? 8 : 0);
      }
      if (!C.clock && !C.held.size && C.chord.length && !recent("chord", 1500)) setChord([]);
      wasm.vis_bpm(C.bpm);
      wasm.vis_tracks(C.tracks);
      wasm.vis_level(Math.round(C.lvl));
      wasm.vis_playing(C.clock || C.lvl > 512 ? 1 : 0);
      wasm.vis_beat(C.clock ? Math.floor((C.ticks % (24 * C.beats)) * 1024 / 24) : 0, C.clock ? C.beats : 0);
      wasm.vis_frame(Math.round(dt));
      draw();
    }
    requestAnimationFrame(frame);
  }
  function draw() {
    const w = wasm.vis_w(), h = wasm.vis_h();
    const px = new Uint8Array(mem.buffer, wasm.vis_px(), w * h), pal = new Uint32Array(mem.buffer, wasm.vis_pal(), 64);
    const d = img.data;
    for (let i = 0; i < w * h; i++) {
      const c = pal[px[i] & 63], o = i * 4;
      d[o] = c >> 16; d[o + 1] = (c >> 8) & 255; d[o + 2] = c & 255; d[o + 3] = 255;
    }
    ctx.putImageData(img, 0, 0);
  }
  function drawFb() {
    const px = new Uint8Array(mem.buffer, wasm.vis_fb(), 240 * 240), pal = new Uint32Array(mem.buffer, wasm.vis_pal(), 64);
    const d = img.data;
    for (let i = 0; i < 240 * 240; i++) {
      const c = pal[px[i] & 63], o = i * 4;
      d[o] = c >> 16; d[o + 1] = (c >> 8) & 255; d[o + 2] = c & 255; d[o + 3] = 255;
    }
    ctx.putImageData(img, 0, 0);
  }
  function playIntro() {
    if (!wasm) return;
    wasm.vis_intro_start();
    cv.width = 240; cv.height = 240;
    img = ctx.createImageData(240, 240);
    intro = { t0: performance.now() };
    fit();
  }
  // the canvas as large as the window allows, its aspect kept (the pixels stay sharp: image-rendering: pixelated)
  function fit() {
    const [w, h] = intro ? [240, 240] : SIZES[size], z = Math.min(innerWidth / w, innerHeight / h);
    cv.style.width = `${Math.floor(w * z)}px`;
    cv.style.height = `${Math.floor(h * z)}px`;
  }
  function setSize(s) {
    size = s;
    const [w, h] = SIZES[s];
    cv.width = w; cv.height = h;
    img = ctx.createImageData(w, h);
    wasm.vis_init(w, h, species, (Math.random() * 2 ** 32) >>> 0);
    setChord(st.chord);
    $("sz0").classList.toggle("on", s === 0); $("sz1").classList.toggle("on", s === 1);
    fit();
  }
  function setSpecies(s) {
    species = s;
    if (wasm) wasm.vis_species(s);
    $("sc0").classList.toggle("on", s === 0); $("sc1").classList.toggle("on", s === 1);
  }
  const fullscreen = () => (document.fullscreenElement ? document.exitFullscreen() : document.documentElement.requestFullscreen());

  // ---- the panel hides when the mouse rests
  let idleT;
  const wake = () => { document.body.classList.remove("idle"); clearTimeout(idleT);
    idleT = setTimeout(() => document.body.classList.add("idle"), 3000); };
  addEventListener("mousemove", wake);
  addEventListener("keydown", (e) => {
    if (e.target.tagName === "INPUT") return;
    const k = e.key.toLowerCase();
    if (k === "f") fullscreen(); else if (k === "t") setSpecies(0); else if (k === "s") setSpecies(1);
    else if (k === "d") demoToggle();
  });
  addEventListener("resize", fit);
  $("fs").onclick = fullscreen; $("demo").onclick = demoToggle;
  $("sc0").onclick = () => setSpecies(0); $("sc1").onclick = () => setSpecies(1);
  $("sz0").onclick = () => setSize(0); $("sz1").onclick = () => setSize(1);
  wake();

  WebAssembly.instantiateStreaming(fetch("sornyak-visuals.wasm"), {}).then(({ instance }) => {
    wasm = instance.exports;
    mem = wasm.memory;
    if (wasm._initialize) wasm._initialize();
    const q = new URLSearchParams(location.search);   // ?demo, ?sornyak, ?device: start that way
    if (q.has("sornyak")) setSpecies(1);
    setSize(q.has("device") ? 1 : 0);
    requestAnimationFrame(frame);
    midiSetup();
    if (q.has("demo")) demoToggle();
    if (q.has("intro")) playIntro();          // (for a look without the synth)
  }).catch((e) => status(`could not load the visuals: ${e}`));
})();
