#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "src/lum_math2d.h"
#include "src/lum_filesystem.h"
#include "src/lum_shader.h"
#include "src/lum_texture.h"
#include "src/lum_renderer2d.h"
#include "src/lum_transform.h"
#include "src/lum_camera.h"
#include "src/lum_window.h"
#include "src/lum_input_codes.h"
#include "src/lum_input.h"
#include "src/lum_time.h"
#include "src/lum_log.h"

int main() {
  lum_window_init("playground", 800, 600);
  lum_input_init();
  lum_time_init();
  lum_log_init();

  lum_renderer2d_init(800, 600, lum_fs_read("shaders/sprite.vert"), lum_fs_read("shaders/sprite.frag"));
  lum_renderer2d_set_viewport(0, 0, (int)lum_window_get_width(), (int)lum_window_get_height());
  lum_renderer2d_enable_blend();
  
  lum_Camera2d camera;
  lum_camera2d_init(&camera, lum_window_get_width(), lum_window_get_height());

  lum_Texture2d texture;
  lum_texture2d_load_from_file(&texture, "resources/texture.jpg", 0);

  lum_Vec2 rect_pos = lum_vec2_create(0.0f, 0.0f);

  lum_time_set_max_fps(144.0f);

  lum_fs_write("test.txt", "Hello luminous!");

  float angle = 0.0f;
  while (lum_window_is_open()) {
    lum_time_begin_frame();
    lum_renderer2d_begin_frame();

    lum_window_poll_events();

    lum_input_update();
	
    lum_camera2d_set_viewport(&camera, lum_window_get_width(), lum_window_get_height());
	
    lum_camera2d_set_zoom(&camera, 0.05f);
    lum_renderer2d_set_camera(&camera);
    

    
	if (lum_input_is_key_down(Lum_Key_Right)) {
	  rect_pos = lum_vec2_add(rect_pos, lum_vec2_scale(lum_vec2_create(2500.0f, 0.0f), lum_time_get_delta()));
	}
	if (lum_input_is_key_down(Lum_Key_Left)) {
	  rect_pos = lum_vec2_add(rect_pos, lum_vec2_scale(lum_vec2_create(-2500.0f, 0.0f), lum_time_get_delta()));
	}
	if (lum_input_is_key_down(Lum_Key_Up)) {
	  rect_pos = lum_vec2_add(rect_pos, lum_vec2_scale(lum_vec2_create(0.0f, 2500.0f), lum_time_get_delta()));
	}
	if (lum_input_is_key_down(Lum_Key_Down)) {
	  rect_pos = lum_vec2_add(rect_pos, lum_vec2_scale(lum_vec2_create(0.0f, -2500.0f), lum_time_get_delta()));
	}

	
	lum_renderer2d_clear();
	lum_renderer2d_draw_rect_ex_layer(rect_pos, lum_vec2_create(2500.0f, 2500.0f), 0.0f, lum_vec2_0(), 2,lum_vec4_1());
	angle += 0.5f * lum_time_get_delta();
  if (angle > Lum_Pi * 2) angle = 0.0f;
  lum_renderer2d_draw_rect_ex_layer(lum_vec2_create(0, 0), lum_vec2_create(2500.0f, 2500.0f), angle, lum_vec2_create(2500, 0), 1, lum_vec4_create(1.0f, 0.5f, 1.0f, 0.3f));
  lum_Vec4 tex_color = lum_vec4_0();
	for (int i = -50000; i < 50000; i += 1000) {
	  for (int j = -50000; j < 50000; j += 1000) {
	    tex_color = lum_vec4_create((float)(i + 50000) / 100000, (float)(j + 50000) / 100000, (float)(i + 50000) / 100000, 1.0f);
	    lum_renderer2d_draw_sprite_ex(&texture, lum_vec2_create((float)i, (float)j), lum_vec2_create(texture.width, texture.height), 0.0f, lum_vec2_0(), tex_color);
	  }
	}
	

	
    lum_renderer2d_end_frame();
    
    lum_window_swap_buffers();
    lum_time_end_frame();
    lum_time_update();
  }

    lum_texture2d_destroy(&texture);

    lum_log_shutdown();
    lum_renderer2d_shutdown();
    lum_input_shutdown();
    lum_window_shutdown();
    return 0;
}
