#include "lum_render_target.h"
#include "lum_log.h"
#include <glad/glad.h>
#include <string.h>
#include "lum_assert.h"

static void lum__rt_reset(lum_Render_target* rt) {
    memset(rt, 0, sizeof(*rt));
}

static void lum__rt_release_gl(lum_Render_target* rt) {
    if (rt->FBO) glDeleteFramebuffers(1, &rt->FBO);
    if (rt->depth_RBO) glDeleteRenderbuffers(1, &rt->depth_RBO);
    rt->FBO = 0;
    rt->depth_RBO = 0;
}

static int lum__rt_build(lum_Render_target* rt, int width, int height, bool depth) {
    if (!lum_texture2d_create(&rt->color, width, height, 4, NULL, Lum_Texture_Flag_Filter_Linear)) {
        Lum_Log_Error("render_target: failed to create color texture.");
        return 0;
    }

    glGenFramebuffers(1, &rt->FBO);
    if (!rt->FBO) {
        lum_texture2d_destroy(&rt->color);
        Lum_Log_Error("render_target: failed to generate FBO.");
        return 0;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, rt->FBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->color.id, 0);

    if (depth) {
        glGenRenderbuffers(1, &rt->depth_RBO);
        if (!rt->depth_RBO) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &rt->FBO);
            rt->FBO = 0;
            lum_texture2d_destroy(&rt->color);
            Lum_Log_Error("render_target: failed to generate depth RBO.");
        }
        glBindRenderbuffer(GL_RENDERBUFFER, rt->depth_RBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rt->depth_RBO);
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        if (rt->depth_RBO) {
            glDeleteRenderbuffers(1, &rt->depth_RBO);
            rt->depth_RBO = 0;
        }
        glDeleteFramebuffers(1, &rt->FBO);
        rt->FBO = 0;
        lum_texture2d_destroy(&rt->color);
        Lum_Log_Error("render_target: FBO incomplete with status 0x%X.", status);
    }

    rt->width = width;
    rt->height = height;
    rt->has_depth = depth;
    return 1;
}

int lum_rt_create(lum_Render_target* rt, int width, int height, bool depth) {
    Lum_Assert(rt);
    if (width <= 0 || height <= 0) {
        Lum_Log_Error("render_target: invalid size %dx%d.", width, height);
        return 0;
    }

    lum__rt_reset(rt);
    if (!lum__rt_build(rt, width, height, depth)) {
        lum__rt_reset(rt);
        return 0;
    }
    rt->initialized = true;
    return 1;
}

void lum_rt_destroy(lum_Render_target* rt) {
    Lum_Assert(rt);
    if (!rt->initialized) return;
    lum__rt_release_gl(rt);
    lum_texture2d_destroy(&rt->color);
    lum__rt_reset(rt);
}

int lum_rt_resize(lum_Render_target* rt, int width, int height) {
    Lum_Assert(rt);
    if (!rt->initialized) return 0;
    if (width <= 0 || height <= 0) {
        Lum_Log_Error("render_target: invalid resize %dx%d.", width, height);
        return 0;
    }
    if (width == rt->width && height == rt->height) return 1;

    bool depth = rt->has_depth;
    lum__rt_release_gl(rt);
    lum_texture2d_destroy(&rt->color);
    rt->initialized = false;
    if (!lum__rt_build(rt, width, height, depth)) {
        lum__rt_reset(rt);
        return 0;
    }
    rt->initialized = true;
    return 1;
}

void lum_rt_bind(const lum_Render_target* rt) {
    Lum_Assert(rt);
    if (!rt->initialized) return;
    glBindFramebuffer(GL_FRAMEBUFFER, rt->FBO);
    glViewport(0, 0, rt->width, rt->height);
}

void lum_rt_bind_screen(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

const lum_Texture2d* lum_rt_get_color(const lum_Render_target* rt) {
    Lum_Assert(rt);
    if (!rt->initialized) return NULL;
    return &rt->color;
}