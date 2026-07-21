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

#include "lum_quad.h"
#include "lum_math2d.h"
#include "lum_file_read.h"
#include "lum_shader.h"
#include "lum_texture.h"
#include "lum_renderer.h"
#include "lum_renderer2d.h"
#include "lum_transform.h"
#include "lum_camera.h"
#include "lum_window.h"
#include "lum_input_codes.h"
#include "lum_input.h"

/* 1.���������                                                                                                         */
/* lum_Vec2��ʾ2ά������lum_Vec3��ʾ3ά������lum_Vec4��ʾ4ά��lum_Mat4��ʾ�Ľ׾���.                                           */
/* ʹ��lum_vecN_create(x, y)����������NָҪ����������ά�ȣ�e.g.����2ά������ʹ��lum_vec2_create����x��y��ʾ������x��y������ʼֵ.    */
/* ʹ��lum_mat4_0���������lum_mat4_identity������λ����.                                                                 */
/* ʾ��: ����3ά�������Ľ׾���                                                                                             */
/* static const lum_Vec3 example_vector = lum_vec3_create(1.0f, 1.0f, 2.0f); */
/* static const lum_Mat4 example_matrix = lum_mat4_identity(); */

int main() {
/* 2.����                                                                                                 */
/* windowģ�鸺��������ں�OpenGL������.                                                                      */
/* ʹ��lum_window_init(title, width, height)��ʼ��windowģ�飬titleָ���ڱ��⣬width��heightָ���ڵĿ��͸�.      */
/* ʹ��lum_window_shutdown�ر�windowģ��.                                                                   */
/* ��ÿ��ѭ��ʱ������lum_window_swap_buffers��������������Ļ������lum_window_poll_events���������¼�.              */
  lum_window_init("playground", 800, 600);
/* 3.���̺�������� */
/* inputģ�鸺��������̺��������. */
/* ʹ��lum_input_init��lum_input_shutdown���򿪺͹ر�inputģ��. */
/* ʹ��lum_input_is_key_down(key)����Ӧ��key�Ƿ񱻰��£������keyֵ��input_codes.h. lum_input_is_mouse_down(button)ͬ��. */
/* ʹ��lum_input_get_mouse_position��ȡ���λ��. */
  lum_input_init();

  lum_Texture2d texture;
  lum_texture2d_load_from_file(&texture, "resources/texture.jpg", 0);

/* 4.��Ⱦ�� */
/* renderer2dģ���ṩͼ����Ⱦ���ϲ�ӿ�. */
/* ʹ��lum_renderer2d_init(width, height, vertex_shader, fragment_shader)��ʼ��renderer2dģ�飬width��heightָ��Ļ���ߣ�vertex_shader��fragment_shader�ֱ�ָ�����ƬԪ��ɫ����ֱ��ʹ�������ֵ����. */
/* ʹ��lum_renderer2d_set_clear_color(color)��������ɫ��ʹ��lum_renderer2d_clear����. */
/* ʹ��lum_renderer2d_draw_rect_ex(pos, size, rotate, origin, color)���ƴ�ɫ���Σ�pos��size��origin�Ƕ�ά������color����ά����. ����posֵ����ԭ��λ�ã�sizeֵ���ο��ߣ�rotateֵ������ԭ����ת�Ļ����ƽǣ�originָ����ԭ������λ�ã�һ���������������������½ǣ���colorָ������ɫ. */
/* ʹ��lum_renderer2d_draw_sprite_ex(texture, ...)����ͼƬ��textureָҪ���Ƶ�����������������ͬ�� */
  char* sprite_vertex_shader = lum_file_read("shaders/sprite.vert");
  char* sprite_fragment_shader = lum_file_read("shaders/sprite.frag");
  lum_renderer2d_init(800, 600, sprite_vertex_shader, sprite_fragment_shader);
  free(sprite_vertex_shader);
  free(sprite_fragment_shader);
  lum_renderer2d_set_viewport(0, 0, (int)lum_window_get_width(), (int)lum_window_get_height());
  lum_renderer2d_enable_blend();
/* 5.��� */
/* camera2dģ��������. */
/* ����ͬʱ���ڶ�������ÿ������ɶ�Ӧ��camera2d�������. ʹ��lum_camera2d_init(camera, width, height)��ʼ��һ�������cameraָ����ʼ�������ָ�룬width��heightָ��Ļ����. */
/* ��ÿ��ѭ���У�ʹ��lum_camera2d_set_viewport������Ļ���ߣ�����ͬ��. */
/* ʹ��lum_camera2d_set_zoom(camera, zoom)����������ţ�zoomָ���ű��ʣ�1.0��ԭ�棬����Խ�ͣ����ų̶�Խ��. */
/* ʹ��lum_camera2d_set_translate��lum_camera2d_set_rotate���������λ�ú���ת�Ƕ�. */
/* ÿ����Ⱦͼ��ʱ����Ҫ����lum_renderer2d_set_camera(camera)ָ����ǰʹ�õ����. */
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

	lum_renderer2d_end_frame();
	lum_window_swap_buffers();
	lum_window_poll_events();

    }

    lum_renderer2d_shutdown();
    lum_texture2d_destroy(&texture);
    lum_input_shutdown();
    lum_window_shutdown();
    return 0;
}
