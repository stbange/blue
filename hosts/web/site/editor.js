export function mountEditor(container, documentText, apply) {
  const patch = JSON.parse(documentText);
  container.replaceChildren();
  const field = (parent, title, input) => {
    const label = document.createElement("label");
    label.append(title, input);
    parent.append(label);
    return input;
  };
  const option = (parent, title, values, current, change) => {
    const input = document.createElement("select");
    for (const item of values) input.add(new Option(item, item));
    input.value = current;
    input.onchange = () => change(input.value);
    return field(parent, title, input);
  };
  const numeric = (parent, title, current, min, max, step, change) => {
    const input = document.createElement("input");
    input.type = "number";
    input.value = current;
    input.min = min;
    input.max = max;
    input.step = step;
    input.onchange = () => {
      const n = input.valueAsNumber;
      if (Number.isFinite(n) && n >= min && n <= max) change(n);
    };
    return field(parent, title, input);
  };
  patch.lanes.forEach((lane, i) => {
    const box = document.createElement("fieldset"),
      legend = document.createElement("legend");
    legend.textContent = "Lane " + "ABC"[i];
    box.append(legend);
    container.append(box);
    option(
      box,
      "Source",
      ["sine", "saw", "table", "fm", "ring", "sample", "grain"],
      lane.source,
      (v) => {
        lane.source = v;
      },
    );
    numeric(box, "Level", lane.gain ?? (i === 0 ? 1 : 0), 0, 2, 0.01, (v) => {
      lane.gain = v;
    });
    numeric(box, "Pan", lane.pan ?? 0, -1, 1, 0.01, (v) => {
      lane.pan = v;
    });
    numeric(box, "Cutoff (Hz)", lane.cutoff ?? 5000, 10, 24000, 1, (v) => {
      lane.cutoff = v;
    });
    for (let n = 0; n < 2; n++)
      option(
        box,
        "Insert " + (n + 1),
        ["bypass", "lowpass", "bandpass", "highpass", "shape", "comb"],
        lane.inserts?.[n] ?? "bypass",
        (v) => {
          lane.inserts ??= ["bypass", "bypass"];
          if (!lane.inserts[0]) lane.inserts[0] = "bypass";
          lane.inserts[n] = v;
        },
      );
    if (lane.source === "table")
      for (let n = 0; n < 2; n++) {
        const amplitudes = document.createElement("input");
        amplitudes.value = (lane.harmonics?.[n] ?? [1]).join(" ");
        amplitudes.onchange = () => {
          const vals = amplitudes.value.trim().split(/\s+/).map(Number);
          if (
            vals.length <= 16 &&
            vals.every((v) => Number.isFinite(v) && Math.abs(v) <= 2)
          ) {
            lane.harmonics ??= [[1], [1]];
            lane.harmonics[n] = vals;
          }
        };
        field(box, "Frame " + (n + 1) + " partial amplitudes", amplitudes);
      }
  });
  const box = document.createElement("fieldset"),
    legend = document.createElement("legend");
  legend.textContent = "Envelope";
  box.append(legend);
  container.append(box);
  for (const [i, point] of (patch.envelope || []).entries()) {
    numeric(
      box,
      "Point " + (i + 1) + " time",
      point.time,
      0,
      3600,
      0.01,
      (v) => (point.time = v),
    );
    numeric(box, "Value", point.value, 0, 1, 0.01, (v) => (point.value = v));
  }
  const add = document.createElement("button");
  add.textContent = "Add breakpoint";
  add.onclick = () => {
    patch.envelope ??= [];
    if (patch.envelope.length < 128) {
      patch.envelope.push({
        time: patch.envelope.length ? patch.envelope.at(-1).time + 1 : 0,
        value: 1,
      });
      mountEditor(container, JSON.stringify(patch), apply);
    }
  };
  box.append(add);
  const commit = document.createElement("button");
  commit.textContent = "Apply sound edits";
  commit.onclick = () => apply(JSON.stringify(patch, null, 2));
  container.append(commit);
}
