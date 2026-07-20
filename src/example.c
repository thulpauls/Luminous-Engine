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

/* 1.向量与矩阵                                                                                                         */
/* lum_Vec2表示2维向量，lum_Vec3表示3维向量，lum_Vec4表示4维，lum_Mat4表示四阶矩阵.                                           */
/* 使用lum_vecN_create(x, y)创建向量，N指要创建向量的维度（e.g.创建2维向量，使用lum_vec2_create）。x，y表示向量的x，y分量初始值.    */
/* 使用lum_mat4_0创建零矩阵，lum_mat4_identity创建单位矩阵.                                                                 */
/* 示例: 创建3维向量和四阶矩阵                                                                                             */
/* static const lum_Vec3 example_vector = lum_vec3_create(1.0f, 1.0f, 2.0f); */
/* static const lum_Mat4 example_matrix = lum_mat4_identity(); */

int main() {
/* 2.窗口                                                                                                 */
/* window模块负责管理窗口和OpenGL上下文.                                                                      */
/* 使用lum_window_init(title, width, height)初始化window模块，title指窗口标题，width和height指窗口的宽和高.      */
/* 使用lum_window_shutdown关闭window模块.                                                                   */
/* 在每次循环时，调用lum_window_swap_buffers来绘制内容至屏幕，调用lum_window_poll_events处理窗口事件.              */
  lum_window_init("playground", 800, 600);
/* 3.键盘和鼠标输入 */
/* input模块负责监听键盘和鼠标输入. */
/* 使用lum_input_init和lum_input_shutdown来打开和关闭input模块. */
/* 使用lum_input_is_key_down(key)检测对应的key是否被按下，具体的key值见input_codes.h. lum_input_is_mouse_down(button)同理. */
/* 使用lum_input_get_mouse_position获取鼠标位置. */
  lum_input_init();

  lum_Texture2d texture;
  lum_texture2d_load_from_file(&texture, "resources/texture.jpg", 0);

/* 4.渲染器 */
/* renderer2d模块提供图形渲染的上层接口. */
/* 使用lum_renderer2d_init(width, height, vertex_shader, fragment_shader)初始化renderer2d模块，width和height指屏幕宽高，vertex_shader和fragment_shader分别指顶点和片元着色器，直接使用下面的值即可. */
/* 使用lum_renderer2d_set_clear_color(color)设置清屏色，使用lum_renderer2d_clear清屏. */
/* 使用lum_renderer2d_draw_rect_ex(pos, size, rotate, origin, color)绘制纯色矩形，pos，size，origin是二维向量，color是四维向量. 其中pos值矩形原点位置，size值矩形宽高，rotate值矩形绕原点旋转的弧度制角，origin指矩形原点的相对位置（一般是零向量，即矩形左下角），color指矩形颜色. */
/* 使用lum_renderer2d_draw_sprite_ex(texture, ...)绘制图片，texture指要绘制的纹理对象，其它参数同上 */
  lum_renderer2d_init(800, 600, lum_file_read("shaders/sprite.vert"), lum_file_read("shaders/sprite.frag"));
  lum_renderer2d_set_viewport(0, 0, (int)lum_window_get_width(), (int)lum_window_get_height());
  lum_renderer2d_enable_blend();
/* 5.相机 */
/* camera2d模块管理相机. */
/* 可以同时存在多个相机，每个相机由对应的camera2d对象管理. 使用lum_camera2d_init(camera, width, height)初始化一个相机，camera指欲初始化相机的指针，width和height指屏幕宽高. */
/* 在每次循环中，使用lum_camera2d_set_viewport更新屏幕宽高，参数同上. */
/* 使用lum_camera2d_set_zoom(camera, zoom)设置相机缩放，zoom指缩放倍率，1.0是原版，倍率越低，缩放程度越高. */
/* 使用lum_camera2d_set_translate和lum_camera2d_set_rotate设置相机的位置和旋转角度. */
/* 每次渲染图形时，都要调用lum_renderer2d_set_camera(camera)指定当前使用的相机. */
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
