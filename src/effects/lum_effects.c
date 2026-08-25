#include "lum_effects.h"
#include "lum_log.h"
#include "lum_assert.h"
#include <string.h>

typedef struct lum_Effects {
  bool initialized;
} lum_Effects;

static lum_Effects g_effects;

int lum_effects_init(void) {
  Lum_Assert(!g_effects.initialized && "lum_effects: double init");
  memset(&g_effects, 0, sizeof(g_effects));

  if (!lum_glow_init()) {
    Lum_Log_Error("effects: glow init failed.");
    return 0;
  }
  if (!lum_beam_init()) {
    lum_glow_shutdown();
    Lum_Log_Error("effects: beam init failed.");
    return 0;
  }

  g_effects.initialized = true;
  return 1;
}

void lum_effects_shutdown(void) {
  if (!g_effects.initialized) return;
  lum_beam_shutdown();
  lum_glow_shutdown();
  memset(&g_effects, 0, sizeof(g_effects));
}

bool lum_effects_is_initialized(void) {
  return g_effects.initialized;
}
