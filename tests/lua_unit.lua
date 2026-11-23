-- Verify the shipped mappings. Unknown Lua files remain in the coverage denominator.
local factory = assert(arg[1])
local banks = {
  formants = {"Vowel", "Bellows", "Ensemble width", "Hall", "macro1/1/morph"},
  littoral = {"Grain activity", "Horizon", "Undertow", "Reflections", "macro1/2/density"},
  nocturnes = {"Bloom", "Orbit", "Unrest", "Afterglow", "macro3/3/pitch"},
  tideline = {"Reed brightness", "Glass presence", "Tide distance", "Grain activity", "macro4/3/density"},
  signals = {"Edge", "Repeat", "Accent", "Spread", "macro2/1/send"},
}
local total, covered = 0, 0
local visited = {}
for _, filename in ipairs(arg) do
  if not visited[filename] then
    visited[filename] = true
    local calls, controls, sources = {}, {}, {}
    local env = { blue = {
      route = function(id, target, depth) calls[id .. "/" .. target] = depth end,
      control = function(id, label)
        assert(not controls[id])
        controls[id] = label
      end,
      mod = function(source, lane, target, depth, rate)
        assert(lane >= 1 and lane <= 3 and math.abs(depth) <= 48)
        if source:match("^lfo") then assert(rate == "control") end
        local key = source .. "/" .. lane .. "/" .. target
        assert(not calls[key])
        calls[key], sources[source] = depth, true
      end
    }}
    local chunk = assert(loadfile(filename, "t", env))
    local info = debug.getinfo(chunk, "SL")
    local file = filename:gsub("\\", "/")
    local bank = file:match("/blue%.(%w+)/scripts/performance%.lua$")
    if file:match("/blue%.soundstudy%.tideline/scripts/tideline%.lua$") then bank = "tideline" end
    local expected = banks[bank]
    local hits = {}
    if filename == factory or expected then
      debug.sethook(function(_, line)
        if debug.getinfo(2, "S").source == info.source then hits[line] = true end
      end, "l")
      chunk()
      debug.sethook()
      if filename == factory then
        assert(calls["1/morph"] == 1 and calls["1/cutoff"] == 3 and calls["1/density"] == 2)
        assert(calls["2/gain"] == -1 and controls[1] == "Motion" and controls[2] == "Level trim")
      else
        for id = 1, 4 do
          assert(controls[id] == expected[id] and sources["macro" .. id])
        end
        assert(sources.pressure and calls[expected[5]] > 0)
      end
    end
    local eligible, count = 0, 0
    for line in pairs(info.activelines) do
      eligible = eligible + 1
      if hits[line] then count = count + 1 end
    end
    print(string.format("%s: %d/%d executable lines", filename, count, eligible))
    total, covered = total + eligible, covered + count
  end
end
print(string.format("Lua total %d/%d", covered, total))
assert(total > 0 and 100 * covered >= 95 * total)
