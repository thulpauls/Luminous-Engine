# Luminous

一个基于 OpenGL 的轻量级 2D 图形框架，使用 C99/C11 编写。

## 构建要求

- CMake 3.10 或更高
- MinGW-w64 或 GCC 工具链（`gcc` / `g++` 需要在 PATH 中）
- Windows

## 构建步骤

```bash
# 1. 生成构建系统
cmake -B build

# 2. 编译
cmake --build build
```

或使用传统 make：

```bash
cd build
make
```

## 示例

```bash
cd build
./Luminous_example
```
注意：`example.c` 会加载 `resources/texture.jpg` 和 `shaders/` 下的着色器，请确保从 `build/` 目录运行（CMake 会在配置时自动复制 `resources` 和 `shaders` 到 `build/`）。
```