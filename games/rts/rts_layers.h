#ifndef Rts_Layers_H
#define Rts_Layers_H

/*
* 渲染层级。
*
* layer 是渲染器的【第一排序键】，越小越先画。这里刻意让所有单位共用同一层 ——
* 给每个单位唯一 layer（做精确 Y 排序）会让每条命令的 layer 都不同，
* 批处理完全失效。M1 用同层 + 同材质/同纹理，让整队单位合成一个 draw call。
*
* Scene 阶段的内容用低层；Screen 阶段（像素空间）的框选与 HUD 必须高于所有 Scene 层。
* 引擎自带的 UI 用 1000（Lum_Ui_Layer），这里从 1040 起错开。
*/
enum {
  Rts_Layer_Units     = 100,   /* 单位本体（由引擎内置 sprite 系统绘制） */
  Rts_Layer_Selection = 900,   /* 选中标记：压在单位之上 */
  Rts_Layer_Box_Fill  = 1040,  /* 框选矩形填充 */
  Rts_Layer_Box_Edge  = 1050,  /* 框选矩形边框 */
  Rts_Layer_Hud       = 1100   /* HUD 文字 */
};

#endif //Rts_Layers_H
