/*
 * OpenVR integration for stereoscopic rendering and curved UI presentation.
 */

#include "vr_openvr.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "args.h"
#include "config.h"
#include "console.h"
#include "inferno.h"
#include "gr.h"

#ifdef OGL
#include <GL/glew.h>
#endif

#ifdef USE_OPENVR
#include <openvr.h>

static vr::IVRSystem *vr_system = NULL;
static vr::IVRCompositor *vr_compositor = NULL;
static bool vr_initialized = false;
static bool vr_gl_ready = false;
static GLuint vr_eye_fbo[2] = {0, 0};
static GLuint vr_eye_color[2] = {0, 0};
static GLuint vr_eye_depth[2] = {0, 0};
static GLuint vr_menu_tex = 0;
static uint32_t vr_render_width = 0;
static uint32_t vr_render_height = 0;
static GLint vr_prev_fbo = 0;
static GLint vr_prev_viewport[4] = {0, 0, 0, 0};

static void vr_openvr_release_gl(void)
{
	if (!vr_gl_ready)
		return;

	glDeleteFramebuffers(2, vr_eye_fbo);
	glDeleteTextures(2, vr_eye_color);
	glDeleteRenderbuffers(2, vr_eye_depth);
	glDeleteTextures(1, &vr_menu_tex);

	vr_eye_fbo[0] = vr_eye_fbo[1] = 0;
	vr_eye_color[0] = vr_eye_color[1] = 0;
	vr_eye_depth[0] = vr_eye_depth[1] = 0;
	vr_menu_tex = 0;
	vr_gl_ready = false;
}

static void vr_openvr_init_render_targets(void)
{
	if (!vr_initialized || vr_gl_ready)
		return;

	if (vr_system) {
		vr_system->GetRecommendedRenderTargetSize(&vr_render_width, &vr_render_height);
	}
	if (vr_render_width == 0 || vr_render_height == 0)
	{
		vr_render_width = (uint32_t)grd_curscreen->sc_w;
		vr_render_height = (uint32_t)grd_curscreen->sc_h;
	}

	for (int eye = 0; eye < 2; eye++)
	{
		glGenTextures(1, &vr_eye_color[eye]);
		glBindTexture(GL_TEXTURE_2D, vr_eye_color[eye]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, vr_render_width, vr_render_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

		glGenRenderbuffers(1, &vr_eye_depth[eye]);
		glBindRenderbuffer(GL_RENDERBUFFER, vr_eye_depth[eye]);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, vr_render_width, vr_render_height);

		glGenFramebuffers(1, &vr_eye_fbo[eye]);
		glBindFramebuffer(GL_FRAMEBUFFER, vr_eye_fbo[eye]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vr_eye_color[eye], 0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, vr_eye_depth[eye]);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			con_printf(CON_NORMAL, "OpenVR framebuffer incomplete for eye %d.\n", eye);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			vr_openvr_release_gl();
			GameCfg.VREnabled = 0;
			return;
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glGenTextures(1, &vr_menu_tex);
	glBindTexture(GL_TEXTURE_2D, vr_menu_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, vr_render_width, vr_render_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	vr_gl_ready = true;
}

static void vr_openvr_draw_curved_quad(GLuint texture)
{
	const int segments = 32;
	const float radius = 2.0f;
	const float curve = 0.7f;
	const float height = 1.4f;

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glUseProgram(0);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-1.0, 1.0, -0.75, 0.75, 0.5, 10.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glBindTexture(GL_TEXTURE_2D, texture);
	glEnable(GL_TEXTURE_2D);

	glBegin(GL_TRIANGLE_STRIP);
	for (int i = 0; i <= segments; i++)
	{
		float t = (float)i / (float)segments;
		float angle = (t - 0.5f) * curve;
		float x = sinf(angle) * radius;
		float z = -cosf(angle) * radius;
		float u = t;

		glTexCoord2f(u, 1.0f);
		glVertex3f(x, -height * 0.5f, z);
		glTexCoord2f(u, 0.0f);
		glVertex3f(x, height * 0.5f, z);
	}
	glEnd();

	glDisable(GL_TEXTURE_2D);
}

static void vr_openvr_draw_flat_quad(GLuint texture)
{
	const float width = 2.0f;
	const float height = 1.2f;
	const float depth = -2.0f;

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glUseProgram(0);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-1.0, 1.0, -0.75, 0.75, 0.5, 10.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glBindTexture(GL_TEXTURE_2D, texture);
	glEnable(GL_TEXTURE_2D);

	glBegin(GL_TRIANGLE_STRIP);
	glTexCoord2f(0.0f, 1.0f);
	glVertex3f(-width * 0.5f, -height * 0.5f, depth);
	glTexCoord2f(0.0f, 0.0f);
	glVertex3f(-width * 0.5f, height * 0.5f, depth);
	glTexCoord2f(1.0f, 1.0f);
	glVertex3f(width * 0.5f, -height * 0.5f, depth);
	glTexCoord2f(1.0f, 0.0f);
	glVertex3f(width * 0.5f, height * 0.5f, depth);
	glEnd();

	glDisable(GL_TEXTURE_2D);
}

#endif

void vr_openvr_init(void)
{
#ifdef USE_OPENVR
	if (!GameCfg.VREnabled || vr_initialized)
		return;

	vr::EVRInitError error = vr::VRInitError_None;
	vr_system = vr::VR_Init(&error, vr::VRApplication_Scene);
	if (error != vr::VRInitError_None)
	{
		con_printf(CON_NORMAL, "OpenVR init failed: %s\n", vr::VR_GetVRInitErrorAsEnglishDescription(error));
		GameCfg.VREnabled = 0;
		return;
	}

	vr_compositor = vr::VRCompositor();
	if (!vr_compositor)
	{
		con_printf(CON_NORMAL, "OpenVR compositor unavailable.\n");
		vr::VR_Shutdown();
		vr_system = NULL;
		GameCfg.VREnabled = 0;
		return;
	}

	vr_initialized = true;
#else
	(void)GameCfg;
#endif
}

void vr_openvr_init_gl(void)
{
#ifdef USE_OPENVR
#ifdef OGL
	if (!vr_initialized)
		return;
	vr_openvr_init_render_targets();
#endif
#endif
}

void vr_openvr_shutdown(void)
{
#ifdef USE_OPENVR
#ifdef OGL
	vr_openvr_release_gl();
#endif
	if (vr_initialized)
	{
		vr::VR_Shutdown();
		vr_initialized = false;
		vr_system = NULL;
		vr_compositor = NULL;
	}
#endif
}

int vr_openvr_active(void)
{
#ifdef USE_OPENVR
	return vr_initialized && GameCfg.VREnabled;
#else
	return 0;
#endif
}

void vr_openvr_begin_frame(void)
{
#ifdef USE_OPENVR
	if (!vr_openvr_active() || !vr_compositor)
		return;

	vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
	vr_compositor->WaitGetPoses(poses, vr::k_unMaxTrackedDeviceCount, NULL, 0);
#endif
}

fix vr_openvr_eye_offset(int eye)
{
#ifdef USE_OPENVR
	if (!vr_openvr_active() || !vr_system)
		return 0;

	vr::Hmd_Eye vr_eye = (eye == 0) ? vr::Eye_Left : vr::Eye_Right;
	vr::HmdMatrix34_t eye_to_head = vr_system->GetEyeToHeadTransform(vr_eye);
	float offset_m = eye_to_head.m[0][3];
	return fl2f(offset_m);
#else
	(void)eye;
	return 0;
#endif
}

void vr_openvr_render_size(int *width, int *height)
{
#ifdef USE_OPENVR
#ifdef OGL
	if (!vr_openvr_active() || !vr_gl_ready)
	{
		if (width)
			*width = 0;
		if (height)
			*height = 0;
		return;
	}

	if (width)
		*width = (int)vr_render_width;
	if (height)
		*height = (int)vr_render_height;
#else
	(void)width;
	(void)height;
#endif
#else
	(void)width;
	(void)height;
#endif
}

void vr_openvr_bind_eye(int eye)
{
#ifdef USE_OPENVR
#ifdef OGL
	if (!vr_openvr_active() || !vr_gl_ready)
		return;

	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &vr_prev_fbo);
	glGetIntegerv(GL_VIEWPORT, vr_prev_viewport);
	glBindFramebuffer(GL_FRAMEBUFFER, vr_eye_fbo[eye]);
	glViewport(0, 0, vr_render_width, vr_render_height);
#else
	(void)eye;
#endif
#else
	(void)eye;
#endif
}

void vr_openvr_unbind_eye(void)
{
#ifdef USE_OPENVR
#ifdef OGL
	if (!vr_openvr_active() || !vr_gl_ready)
		return;

	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)vr_prev_fbo);
	glViewport(vr_prev_viewport[0], vr_prev_viewport[1], vr_prev_viewport[2], vr_prev_viewport[3]);
#endif
#endif
}

void vr_openvr_submit_eyes(void)
{
#ifdef USE_OPENVR
#ifdef OGL
	if (!vr_openvr_active() || !vr_compositor || !vr_gl_ready)
		return;

	vr::Texture_t left = {(void *)(uintptr_t)vr_eye_color[0], vr::TextureType_OpenGL, vr::ColorSpace_Auto};
	vr::Texture_t right = {(void *)(uintptr_t)vr_eye_color[1], vr::TextureType_OpenGL, vr::ColorSpace_Auto};
	vr_compositor->Submit(vr::Eye_Left, &left);
	vr_compositor->Submit(vr::Eye_Right, &right);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, vr_eye_fbo[0]);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	GLint blit_width = (GLint)vr_render_width;
	GLint blit_height = (GLint)vr_render_height;
	if (blit_width > grd_curscreen->sc_w)
		blit_width = grd_curscreen->sc_w;
	if (blit_height > grd_curscreen->sc_h)
		blit_height = grd_curscreen->sc_h;
	glBlitFramebuffer(0, 0, blit_width, blit_height, 0, 0, grd_curscreen->sc_w, grd_curscreen->sc_h, GL_COLOR_BUFFER_BIT, GL_LINEAR);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
#endif
#endif
}

void vr_openvr_submit_mono_from_screen(int curved)
{
#ifdef USE_OPENVR
#ifdef OGL
	if (!vr_openvr_active() || !vr_gl_ready || !vr_compositor)
		return;

	vr_openvr_begin_frame();
	GLint prev_framebuffer = 0;
	GLint prev_read_buffer = GL_BACK;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_framebuffer);
	glGetIntegerv(GL_READ_BUFFER, &prev_read_buffer);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glReadBuffer(GL_BACK);
	glBindTexture(GL_TEXTURE_2D, vr_menu_tex);
	GLint copy_width = (GLint)vr_render_width;
	GLint copy_height = (GLint)vr_render_height;
	if (copy_width > grd_curscreen->sc_w)
		copy_width = grd_curscreen->sc_w;
	if (copy_height > grd_curscreen->sc_h)
		copy_height = grd_curscreen->sc_h;
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, copy_width, copy_height);
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_framebuffer);
	glReadBuffer(prev_read_buffer);

	for (int eye = 0; eye < 2; eye++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, vr_eye_fbo[eye]);
		glViewport(0, 0, vr_render_width, vr_render_height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		if (curved)
			vr_openvr_draw_curved_quad(vr_menu_tex);
		else
			vr_openvr_draw_flat_quad(vr_menu_tex);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	vr_openvr_submit_eyes();
#endif
#endif
}
