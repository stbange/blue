export const keys = "zxcvbnmasdfghjklqwertyuiop1234567890";
const names = ["C", "C♯", "D", "D♯", "E", "F", "F♯", "G", "G♯", "A", "A♯", "B"];

export function pitch(tuning, step) {
  const { root, cents } = tuning;
  return root + 12 * Math.floor(step / cents.length) + cents[step % cents.length] / 100;
}

export function pitchName(pitch) {
  const tenths = Math.round(pitch * 1000);
  const note = Math.floor(tenths / 1000);
  const cents = (tenths % 1000) / 10;
  const name = names[note % 12] + (Math.floor(note / 12) - 1);
  return cents ? name + "+" + cents + "¢" : name;
}

export function validTuning(tuning) {
  const { root, cents } = tuning || {};
  return Number.isInteger(root) && root >= 0 &&
    Array.isArray(cents) && cents.length >= 4 && cents.length <= 24 &&
    cents[0] === 0 && cents.every((v, i) =>
      Number.isFinite(v) && v < 1200 && (!i || v > cents[i - 1])) &&
    pitch(tuning, keys.length - 1) <= 127;
}

export function parseTuning(root, intervals) {
  const parts = intervals.trim().split(/\s*,\s*|\s+/);
  const tuning = {
    root: Number(root),
    cents: parts.map((part) => /^[0-9]+(?:\.[0-9]+)?$/.test(part) ? Number(part) : NaN),
  };
  return root.trim() && validTuning(tuning) ? tuning : null;
}

export const defaultTuning = { root: 36, cents: [0, 200, 400, 500, 700, 900, 1100] };
