import { exportPack, unpack } from "./packs.js";
import { mountEditor } from "./editor.js";
import { enterXR } from "./xr.js";
import {
  defaultTuning, keys, parseTuning, pitch, pitchName, validTuning,
} from "./tuning.js";
const $ = (id) => document.getElementById(id);
const status = (message) => ($("status").textContent = message);
const worker = new Worker("worker.js");
let context,
  node,
  recorder,
  analyser,
  capture = false,
  serial = 0,
  prepared,
  root,
  name,
  manifest;
let requested,
  active,
  ready = false,
  starting = false;
let assets;
const packs = new Map();
const storageKey = (preset) => JSON.stringify([$("pack").value, preset]);
const values = Array(8).fill(0);
const mappings = [
  "Motion",
  "Level trim",
  "Macro 3",
  "Macro 4",
  "Macro 5",
  "Macro 6",
  "Macro 7",
  "Macro 8",
];
const held = keys.split("").map(() => new Set());
const pressed = new Map();
const tunedNoteOn = 11, firstKeyboardId = 128;
const cells = [];
let tuning = defaultTuning;
function event(c, i, v, when) {
  if (!node) return;
  const delta = Number.isFinite(when) ? (when-performance.now())/1000 : 0;
  const frame = Math.round((context.currentTime + delta) * context.sampleRate) + 128;
  node.port.postMessage({c, i, v, frame});
}
function note(step, source, on) {
  const sources = held[step], wasHeld = sources.size > 0;
  if (on) {
    if (!node) return;
    sources.add(source);
  } else sources.delete(source);
  const isHeld = sources.size > 0;
  if (isHeld === wasHeld) return;
  event(
    isHeld ? tunedNoteOn : 1,
    firstKeyboardId + step,
    isHeld ? pitch(tuning, step) : 0,
  );
  cells[step].button.classList.toggle("held", isHeld);
}
function panic() {
  pressed.clear();
  for (const [step, sources] of held.entries()) {
    sources.clear();
    cells[step].button.classList.remove("held");
  }
  event(7, 0, 0);
}
function macro(i, v) {
  values[i] = v;
  $("m" + i).value = v;
  event(6, i, v);
}
for (let i = 0; i < 8; i++) {
  const label = document.createElement("label"),
    slider = document.createElement("input");
  label.textContent = mappings[i];
  slider.type = "range";
  slider.min = 0;
  slider.max = 1;
  slider.step = 0.01;
  slider.value = 0;
  slider.id = "m" + i;
  slider.oninput = () => macro(i, Number(slider.value));
  label.append(slider);
  $("macros").append(label);
}
for (const row of ["1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"]) {
  const group = document.createElement("div");
  group.className = "key-row";
  for (const key of row) {
    const step = keys.indexOf(key), button = document.createElement("button");
    const letter = document.createElement("strong"), tone = document.createElement("span");
    letter.textContent = key.toUpperCase();
    button.type = "button";
    button.id = "key-" + key;
    button.append(letter, tone);
    button.onpointerdown = (e) => {
      button.setPointerCapture(e.pointerId);
      note(step, e.pointerId, true);
    };
    button.onlostpointercapture = (e) => note(step, e.pointerId, false);
    button.onclick = (e) => {
      if (e.detail === 0) {
        note(step, button, true);
        setTimeout(() => note(step, button, false), 180);
      }
    };
    group.append(button);
    cells[step] = { button, tone };
  }
  $("keyboard").append(group);
}
function keyboard(e, on) {
  const key = e.key.toLowerCase(), source = e.code || key;
  if (!on) {
    const step = pressed.get(source);
    if (step !== undefined) {
      pressed.delete(source);
      note(step, source, false);
    }
    return;
  }
  if (
    e.target.closest("input,textarea,select,[contenteditable]") ||
    e.ctrlKey || e.metaKey || e.altKey || !node || pressed.has(source)
  ) return;
  const symbol = /^Digit[0-9]$/.test(source) ? source.slice(-1) : key;
  const step = symbol.length === 1 ? keys.indexOf(symbol) : -1;
  if (step >= 0) {
    e.preventDefault();
    pressed.set(source, step);
    note(step, source, true);
  }
}
window.onkeydown = (e) => keyboard(e, true);
window.onkeyup = (e) => keyboard(e, false);
window.onblur = panic;

function showTuning(next) {
  tuning = next;
  for (const [step, { button, tone }] of cells.entries()) {
    tone.textContent = pitchName(pitch(tuning, step));
    button.setAttribute("aria-label", keys[step].toUpperCase() + " " + tone.textContent);
  }
  $("tuning-range").textContent =
    pitchName(pitch(tuning, 0)) + " – " + pitchName(pitch(tuning, keys.length - 1));
  $("tuning-root").value = tuning.root;
  $("tuning-cents").value = tuning.cents.join(", ");
  $("tuning-error").hidden = true;
}
async function initTuning() {
  const select = $("tuning");
  let tunings = [{ ...defaultTuning, name: "C major (C Dur)" }], saved;
  try {
    const response = await fetch("content/tunings.json");
    if (response.ok) {
      const catalog = (await response.json()).filter(
        (t) => validTuning(t) && typeof t.name === "string",
      );
      if (catalog.length) tunings = catalog;
    }
  } catch {
    // The default keyboard is available even if the shared catalog cannot load.
  }
  try {
    saved = JSON.parse(window.localStorage.getItem("blue-tuning"));
  } catch {
    // Private browsing or an outdated saved value can prevent restoration.
  }
  let custom = validTuning(saved?.custom) ? saved.custom : { ...tunings[0] };
  const selected = saved?.selected;
  for (const t of tunings) select.add(new Option(t.name, t.name));
  select.add(new Option("Custom", ""));
  if (selected === "" || tunings.some((t) => t.name === selected))
    select.value = selected;
  function choose() {
    showTuning(tunings.find((t) => t.name === select.value) || custom);
    try {
      window.localStorage.setItem(
        "blue-tuning", JSON.stringify({ selected: select.value, custom }),
      );
    } catch {
      // Playable for this session even when storage is disabled.
    }
  }
  select.onchange = choose;
  $("tuning-apply").onclick = () => {
    const candidate = parseTuning($("tuning-root").value, $("tuning-cents").value);
    if (!candidate) {
      const error = $("tuning-error");
      error.textContent = "Use a whole MIDI root and 4–24 ascending steps starting at 0. All 36 notes must fit in MIDI 0–127.";
      error.hidden = false;
      return;
    }
    custom = candidate;
    select.value = "";
    choose();
  };
  choose();
}
showTuning(defaultTuning);
void initTuning();
function database() {
  return new Promise((resolve, reject) => {
    const req = indexedDB.open("BlueRevisions", 1);
    req.onupgradeneeded = () => req.result.createObjectStore("patches");
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
  });
}
async function storage(key, value) {
  const db = await database();
  try {
    return await new Promise((resolve, reject) => {
      const tx = db.transaction("patches", value ? "readwrite" : "readonly"),
        s = tx.objectStore("patches");
      const req = value ? s.put(value, key) : s.get(key);
      tx.oncomplete = () => resolve(req.result);
      tx.onerror = () => reject(tx.error);
    });
  } finally {
    db.close();
  }
}
async function select(preset, revision, fromDisk = false) {
  const id = ++serial;
  requested = preset;
  status("Loading " + preset + "…");
  try {
    if (!revision && !fromDisk)
      revision = await storage(storageKey(preset)).catch(() => undefined);
    if (id !== serial) return;
    worker.postMessage({
      id,
      root,
      preset,
      assets,
      patch: revision?.patch,
      script: revision?.script,
    });
    active = revision?.macros ?? (fromDisk && preset === name ? [...values] : undefined);
  } catch (e) {
    status(e.message);
  }
}
function controls() {
  name = requested;
  $("name").textContent = name;
  $("document").value = prepared.document;
  $("mapping").value = prepared.mapping;
  mountEditor($("sound-editor"), prepared.document, (patch) =>
    select(name, { ...revision(), patch }),
  );
  $("lanes").replaceChildren(
    ...JSON.parse(prepared.document).lanes.map((l, i) => {
      const s = document.createElement("span");
      s.textContent = "ABC"[i] + "  /  " + l.source;
      return s;
    }),
  );
  for (const b of $("library").children)
    b.setAttribute("aria-current", String(b.textContent === name));
  prepared.labels.forEach((label, i) => {
    $("m" + i).parentNode.firstChild.nodeValue = label || mappings[i];
  });
  const initial = active ?? JSON.parse(prepared.document).macros ?? [];
  for (let i = 0; i < 8; i++) macro(i, initial[i] ?? 0);
}
worker.onmessage = ({ data }) => {
  if (data.id !== serial) return;
  if (data.error) {
    status(data.error);
    return;
  }
  prepared = data;
  controls();
  if (node) publish();
  else {
    ready = true;
    status("Patch ready. Start audio to play.");
  }
};
worker.onerror = (e) => status(e.message);
function publish() {
  node.port.postMessage({ id: serial, buffer: prepared.buffer });
}
async function start() {
  if (starting || node) return;
  if (!ready) {
    status("Wait for the patch to load.");
    return;
  }
  starting = true;
  status("Starting audio device…");
  try {
    context = new AudioContext();
    const resume = context.resume();
    status("Loading audio renderer…");
    const [module, worklet] = await Promise.all([
      WebAssembly.compileStreaming(fetch("audio.wasm")),
      fetch("worklet.js").then((r) => r.text()),
    ]);
    const workletURL = URL.createObjectURL(
      new Blob([worklet], { type: "text/javascript" }),
    );
    status("Initializing audio worklet…");
    try {
      await context.audioWorklet.addModule(workletURL);
    } finally {
      URL.revokeObjectURL(workletURL);
    }
    node = new AudioWorkletNode(context, "blue-render", {
      numberOfInputs: 0,
      outputChannelCount: [2],
      processorOptions: { module, voices: /(?:^\?|&)voices=8(?:&|$)/.test(window.location?.search || "") ? 8 : 16 },
    });
    recorder = new AudioWorkletNode(context, "blue-recorder", {
      outputChannelCount: [2],
    });
    analyser = context.createAnalyser();
    analyser.fftSize = 1024;
    node.connect(recorder).connect(analyser).connect(context.destination);
    recorder.port.onmessage = ({ data }) => {
      download(wav(data.samples, data.rate), "Blue-recording.wav");
      status("Recording saved.");
    };
    node.port.onmessage = ({ data }) => {
      if (data.applied === serial) {
        values.forEach((v, i) => event(6, i, v));
        status("Playing " + name);
      }
    };
    status("Waiting for audio output…");
    publish();
    await resume;
    $("start").textContent = "Audio on";
    $("start").disabled = true;
  } catch (e) {
    status(e.message);
    node = null;
    if (context) await context.close();
  } finally {
    starting = false;
  }
}
$("start").onclick = start;
function download(blob, filename) {
  const url = URL.createObjectURL(blob),
    a = document.createElement("a");
  a.href = url;
  a.download = filename;
  a.click();
  setTimeout(() => URL.revokeObjectURL(url), 30000);
}
function wav(samples, rate) {
  const buf = new ArrayBuffer(44 + samples.length * 2),
    v = new DataView(buf);
  const str = (offset, s) =>
    [...s].forEach((c, i) => v.setUint8(offset + i, c.charCodeAt(0)));
  str(0, "RIFF");
  v.setUint32(4, buf.byteLength - 8, true);
  str(8, "WAVEfmt ");
  v.setUint32(16, 16, true);
  v.setUint16(20, 1, true);
  v.setUint16(22, 2, true);
  v.setUint32(24, rate, true);
  v.setUint32(28, rate * 4, true);
  v.setUint16(32, 4, true);
  v.setUint16(34, 16, true);
  str(36, "data");
  v.setUint32(40, samples.length * 2, true);
  samples.forEach((x, i) =>
    v.setInt16(
      44 + i * 2,
      Math.round(Math.max(-1, Math.min(1, x)) * 32767),
      true,
    ),
  );
  return new Blob([buf], { type: "audio/wav" });
}
$("record").onclick = () => {
  if (!node) return status("Start audio to record.");
  capture = !capture;
  recorder.port.postMessage(capture);
  $("record").textContent = capture ? "Stop recording" : "Record output";
};
const revision = () => ({
  format: 1,
  preset: name,
  patch: $("document").value,
  script: $("mapping").value,
  macros: [...values],
});
$("apply").onclick = () => select(name, revision());
$("save").onclick = async () => {
  if (!prepared) return;
  try {
    await storage(storageKey(name), {
      ...revision(),
      patch: prepared.document,
      script: prepared.mapping,
    });
    status("Saved active revision.");
  } catch (e) {
    status(e.message);
  }
};
$("reload").onclick = () => select(name || requested, undefined, true);
$("export").onclick = async () => {
  try {
    const read = async (file) => {
      if (assets) return assets[file];
      const r = await fetch(new URL(file, root));
      if (!r.ok) throw Error(file);
      return new Uint8Array(await r.arrayBuffer());
    };
    download(
      await exportPack(
        manifest,
        name,
        prepared.document,
        prepared.mapping,
        values,
        read,
      ),
      name + ".zip",
    );
  } catch (e) {
    status(e.message);
  }
};
$("import").onchange = async (e) => {
  try {
    const item = unpack(new Uint8Array(await e.target.files[0].arrayBuffer()));
    const key = "user/" + item.manifest.id;
    await storage(key, item);
    const users = (await storage("imports")) || [];
    if (!users.includes(key)) await storage("imports", [...users, key]);
    register(key, item);
    openPack(key);
  } catch (err) {
    status(err.message);
  } finally {
    e.target.value = "";
  }
};
$("midi").onclick = async () => {
  try {
    const midi = await navigator.requestMIDIAccess();
    for (const input of midi.inputs.values())
      input.onmidimessage = ({ data: [s, k, v] }) => {
        switch (s >> 4) {
          case 8:
            event(1, k, 0);
            break;
          case 9:
            event(v ? 0 : 1, k, v / 127);
            break;
          case 11:
            if (k === 64) event(2, 0, v / 127);
            else if (k === 1) event(5, 0, v / 127);
            break;
          case 13:
            event(4, 0, k / 127);
            break;
          case 14:
            event(3, 0, (v * 128 + k - 8192) / 4096);
            break;
        }
      };
    status("MIDI connected.");
  } catch (e) {
    status(e.message);
  }
};
$("xr").onclick = () => {
  if (!node) return status("Start audio before entering WebXR.");
  enterXR(event, status, macro).catch((e) => status(e.message));
};
const canvas = $("scope"),
  g = canvas.getContext("2d"),
  signal = new Float32Array(1024);
function paint() {
  if (analyser) analyser.getFloatTimeDomainData(signal);
  g.clearRect(0, 0, 900, 120);
  g.strokeStyle = "#294859";
  g.beginPath();
  for (let x = 0; x < 900; x += 50) {
    g.moveTo(x, 0);
    g.lineTo(x, 120);
  }
  g.stroke();
  g.beginPath();
  g.strokeStyle = "#75dfdc";
  g.lineWidth = 2;
  for (let i = 0; i < signal.length; i++) {
    const x = (i * 900) / 1023,
      y = 60 - signal[i] * 108;
    i ? g.lineTo(x, y) : g.moveTo(x, y);
  }
  g.stroke();
  requestAnimationFrame(paint);
}
paint();
function register(key, item) {
  if (!packs.has(key))
    $("pack").add(
      new Option(item.manifest.id + (item.assets ? " (user)" : ""), key),
    );
  packs.set(key, item);
}
function openPack(key) {
  const item = packs.get(key);
  if (!item) return;
  manifest = item.manifest;
  assets = item.assets;
  root = item.root;
  $("pack").value = key;
  $("library").replaceChildren();
  for (const preset of Object.keys(manifest.presets)) {
    const b = document.createElement("button");
    b.textContent = preset;
    b.onclick = () => select(preset);
    $("library").append(b);
  }
  select(Object.keys(manifest.presets)[0]);
}
$("pack").onchange = () => openPack($("pack").value);
try {
  const index = await (await fetch("content/index.json")).json();
  for (const folder of index.packs) {
    const url = new URL("content/" + folder + "/", document.baseURI).href;
    register(folder, {
      root: url,
      manifest: await (await fetch(new URL("pack.json", url))).json(),
    });
  }
  for (const key of (await storage("imports").catch(() => [])) || []) {
    const item = await storage(key);
    if (item) register(key, item);
  }
  openPack(index.packs[0]);
} catch (e) {
  status(e.message);
}
