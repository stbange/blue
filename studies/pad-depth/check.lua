local routes, labels = {}, {}
local env = {blue = {
  control = function(id, label)
    assert(not labels[id])
    labels[id] = label
  end,
  mod = function(source, lane, target, depth, rate)
    local key = source .. "/" .. lane .. "/" .. target
    assert(not routes[key] and lane >= 1 and lane <= 3)
    if source:match("^lfo") then assert(rate == "control") end
    routes[key] = depth
  end
}}
local mapping = assert(loadfile(arg[1], "t", env))
local info = debug.getinfo(mapping, "SL")
local hit = {}
debug.sethook(function(_, line)
  if debug.getinfo(2, "S").source == info.source then hit[line] = true end
end, "l")
mapping()
debug.sethook()
assert(table.concat(labels, ",") == "Reed brightness,Glass presence,Tide distance,Grain activity")
assert(routes["lfo2/2/level"] < 0 and routes["lfo2/3/level"] > 0)
assert(routes["lfo3/2/pan"] * routes["lfo3/3/pan"] < 0)
assert(routes["macro1/1/morph"] > 0 and routes["macro2/2/level"] > 0)
assert(routes["macro3/3/send"] > 0 and routes["macro4/3/density"] > 0)
assert(routes["pressure/2/level"] > 0)
local total = 0
for line in pairs(info.activelines) do
  assert(hit[line], "missed mapping line " .. line)
  total = total + 1
end
print(string.format("Tideline mapping: %d/%d executable lines", total, total))
