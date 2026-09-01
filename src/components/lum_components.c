#include "components/lum_components.h"

int lum_components_register(lum_Ecs* ecs) {
  if (!ecs) return 0;
  if (lum_component_glow_register(ecs) == Lum_Component_Invalid) return 0;
  if (lum_component_beam_register(ecs) == Lum_Component_Invalid) return 0;
  return 1;
}
