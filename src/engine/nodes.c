#include "blue_node.h"
#include <stddef.h>
#include <math.h>
#include <string.h>
#define MAX_TYPES 64
static struct {uint32_t key;const BlueNodeType *type;} registry[MAX_TYPES];
static size_t count;
uint32_t blue_node_key(const char *name){
    if(!name||!*name)return 0;
    uint32_t h=2166136261u;
    for(const unsigned char *c=(const unsigned char*)name;*c;c++)h=(h^*c)*16777619u;
    return h;
}
const BlueNodeType *blue_node(uint32_t key){
    for(size_t i=0;i<count;i++)if(registry[i].key==key)return registry[i].type;
    return NULL;
}
int blue_register_node(const BlueNodeType *type){
    if(!type || !type->type_id || !type->process || !type->parameter_name || count==MAX_TYPES)return 0;
    uint32_t key=blue_node_key(type->type_id);
    if(!key || blue_node(key) || !isfinite(type->initial) || !(type->initial>=type->minimum && type->initial<=type->maximum))return 0;
    registry[count].key=key;registry[count++].type=type;return 1;
}
