#ifndef Lum_Scene_Io_H
#define Lum_Scene_Io_H

#include "lum_ecs.h"

int lum_scene_io_init(lum_Ecs* ecs);

int lum_scene_save(const lum_Ecs* ecs, const char* path);
int lum_scene_load(lum_Ecs* ecs, const char* path);

#endif //Lum_Scene_Io_H
