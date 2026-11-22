#include "blue_node.h"
static float render(double *state, double phase, double hz, int rate,
                    float value) {
  (void)state;
  (void)hz;
  (void)rate;
  return phase < .5 ? value : -value;
}
void blue_register_extensions(void) {
  static const BlueNodeType pulse = {
      "fixture.pulse.v1", "amplitude", 0, 1, .5, 0, render};
  blue_register_node(&pulse);
}
