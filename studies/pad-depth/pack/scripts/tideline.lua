-- Keep the reed close to the center. Its two formants trade emphasis slowly.
blue.mod("lfo1", 1, "morph", 0.36, "control")
blue.mod("lfo2", 1, "cutoff", 0.42, "control")
blue.mod("lfo3", 1, "pan", 0.09, "control")

-- The glass layer recedes as the grain layer comes forward. A few cents of
-- pitch drift create beating against the sustained reed.
blue.mod("lfo2", 2, "level", -0.85, "control")
blue.mod("lfo1", 2, "pitch", 0.095, "control")
blue.mod("lfo3", 2, "pan", 0.53, "control")
blue.mod("lfo2", 3, "level", 0.72, "control")
blue.mod("lfo1", 3, "density", -0.34, "control")
blue.mod("lfo3", 3, "pan", -0.56, "control")

blue.control(1, "Reed brightness")
blue.mod("macro1", 1, "morph", 0.38)
blue.mod("macro1", 1, "cutoff", 1.9)
blue.control(2, "Glass presence")
blue.mod("macro2", 2, "level", 1.15)
blue.mod("macro2", 2, "pitch", 0.09)
blue.control(3, "Tide distance")
blue.mod("macro3", 3, "space", 0.38)
blue.mod("macro3", 3, "send", 0.33)
blue.mod("macro3", 3, "level", -0.34)
blue.control(4, "Grain activity")
blue.mod("macro4", 3, "density", 0.9)
blue.mod("macro4", 3, "level", 0.48)
blue.mod("pressure", 2, "level", 0.64)
