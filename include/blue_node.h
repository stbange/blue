#ifndef BLUE_NODE_H
#define BLUE_NODE_H
#include <stdint.h>
/* Register descriptors with static lifetime before opening any audio device.
   IDs include a namespace and version, e.g. "example.bowed.v1". */
typedef struct {
  const char *type_id;
  const char *parameter_name;
  float minimum, maximum, initial;
  void (*reset)(double state[8]);
  float (*process)(double state[8], double phase, double hz, int rate,
                   float parameter);
} BlueNodeType;
void blue_initialize_nodes(void);
/* A BLUE_EXTENSION_SOURCE build supplies this entry point. */
void blue_register_extensions(void);
int blue_register_node(const BlueNodeType *type);
const BlueNodeType *blue_node(uint32_t key);
uint32_t blue_node_key(const char *name);
#endif
