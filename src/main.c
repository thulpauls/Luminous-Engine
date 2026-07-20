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

#include "quad.h"
#include "math2d.h"
#include "file_read.h"
#include "shader.h"
#include "texture.h"
#include "renderer.h"
#include "renderer2d.h"
#include "transform.h"
#include "camera.h"
#include "window.h"
#include "input_codes.h"
#include "input.h"

int main() {
  lum_window_init("playground", 800, 600);
  lum_input_init();

  lum_Texture2d texture;
  lum_texture2d_load_from_file(&texture, "resources/texture.jpg", 0);

  lum_renderer2d_init(800, 600, lum_file_read("shaders/sprite.vert"), lum_file_read("shaders/sprite.frag"));
  lum_renderer2d_set_viewport(0, 0, (int)lum_window_get_width(), (int)lum_window_get_height());
  lum_renderer2d_enable_blend();
  
  lum_Camera2d camera;
  lum_camera2d_init(&camera, lum_window_get_width(), lum_window_get_height());
  
  while (lum_window_is_open()) {
	lum_renderer2d_begin_frame();

	lum_input_update();
	
	lum_camera2d_set_viewport(&camera, lum_window_get_width(), lum_window_get_height());
	lum_renderer2d_set_clear_color(lum_vec4_create(0.0f, 0.3f, 0.3f, 1.0f));
	lum_renderer2d_clear();
	
	lum_camera2d_set_zoom(&camera, 0.2f);
	lum_renderer2d_set_camera(&camera);
	
	lum_renderer2d_draw_rect_ex(lum_vec2_create(0.0f, 0.0f), lum_vec2_create(200.0f, 200.0f), 0.0f, lum_vec2_0(), !lum_input_is_mouse_down(Lum_Mouse_Button_1) ? lum_vec4_create(0, 0, 0, 0.5f) : lum_vec4_1());
	lum_renderer2d_draw_sprite_ex(&texture, lum_vec2_create(-230.0f, 1000.0f), lum_vec2_create(texture.width, texture.height), 0.0f, lum_vec2_0(), lum_vec4_1()); 

	lum_window_swap_buffers();
	lum_window_poll_events();

	lum_renderer2d_end_frame();
    }

    lum_renderer2d_shutdown();
    lum_texture2d_destroy(&texture);
    lum_input_shutdown();
    lum_window_shutdown();
    return 0;
}
