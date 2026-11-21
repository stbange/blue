-- Run the delivered factory mapping with the same public API and check every route.
local filename = assert(arg[1])
local calls, controls = {}, {}
local env = { blue = {
  route = function(id, destination, depth)
    calls[id .. "/" .. destination] = depth
  end,
  control = function(id, label)
    controls[id] = label
  end,
} }
local chunk = assert(loadfile(filename, "t", env))
local info = debug.getinfo(chunk, "SL")
local hits = {}
debug.sethook(function(_, line)
  if debug.getinfo(2, "S").source == info.source then hits[line] = true end
end, "l")
chunk()
debug.sethook()
assert(calls["1/morph"] == 1 and calls["1/cutoff"] == 3 and calls["1/density"] == 2)
assert(calls["2/gain"] == -1 and controls[1] == "Motion" and controls[2] == "Level trim")
local covered, eligible = 0, 0
for line in pairs(info.activelines) do
  eligible = eligible + 1
  if hits[line] then covered = covered + 1 end
end
print(string.format("%s: %d/%d executable lines", filename, covered, eligible))
assert(eligible > 0 and 100 * covered >= 95 * eligible)
