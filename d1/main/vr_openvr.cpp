/*
 * OpenVR integration for stereoscopic rendering and curved UI presentation.
 */

#include "vr_openvr.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

extern "C" {
#include "args.h"
#include "config.h"
#include "console.h"
#include "inferno.h"
#include "gr.h"
#include "gamefont.h"
extern int last_width, last_height;
extern int sdl_window_width, sdl_window_height;
#ifdef _WIN32
const vms_matrix vmd_identity_matrix = IDENTITY_MATRIX;
#endif
}

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
static GLuint vr_menu_fbo = 0;
static GLuint vr_menu_tex = 0;
static uint8_t *vr_menu_pixels = NULL;
static size_t vr_menu_pixels_size = 0;
static uint32_t vr_render_width = 0;
static uint32_t vr_render_height = 0;
static GLint vr_prev_fbo = 0;
static GLint vr_prev_viewport[4] = {0, 0, 0, 0};
static int Canvas_width = 0;
static int Canvas_height = 0;
static int vr_prev_screen_width = 0;
static int vr_prev_screen_height = 0;
static int vr_prev_canvas_bm_w = 0;
static int vr_prev_canvas_bm_h = 0;
static int vr_prev_canvas_bm_rowsize = 0;
static int vr_prev_canvas_width = 0;
static int vr_prev_canvas_height = 0;
static int vr_prev_last_width = 0;
static int vr_prev_last_height = 0;
static float vr_eye_offset_adjust_m = 0.0f;
static int vr_current_eye = -1;
static bool vr_has_pose = false;
static vms_matrix vr_head_orient = vmd_identity_matrix;
static vms_vector vr_head_pos = {0, 0, 0};
static bool vr_skip_mirror_blit = false;

static void vr_openvr_release_gl(void)
{
	if (!vr_gl_ready)
		return;

	glDeleteFramebuffers(2, vr_eye_fbo);
	glDeleteTextures(2, vr_eye_color);
	glDeleteRenderbuffers(2, vr_eye_depth);
	glDeleteFramebuffers(1, &vr_menu_fbo);
	glDeleteTextures(1, &vr_menu_tex);

	vr_eye_fbo[0] = vr_eye_fbo[1] = 0;
	vr_eye_color[0] = vr_eye_color[1] = 0;
	vr_eye_depth[0] = vr_eye_depth[1] = 0;
	vr_menu_fbo = 0;
	vr_menu_tex = 0;
	free(vr_menu_pixels);
	vr_menu_pixels = NULL;
	vr_menu_pixels_size = 0;
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
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, vr_render_width, vr_render_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	glGenFramebuffers(1, &vr_menu_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, vr_menu_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vr_menu_tex, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		con_printf(CON_NORMAL, "OpenVR framebuffer incomplete for menu.\n");
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		vr_openvr_release_gl();
		GameCfg.VREnabled = 0;
		return;
	}
	vr_gl_ready = true;
}

static void vr_openvr_apply_eye_modelview(int eye)
{
	if (eye < 0)
		return;

	const float eye_offset = f2fl(vr_openvr_eye_offset(eye));
	glTranslatef(eye_offset, 0.0f, 0.0f);
}

static void vr_openvr_draw_curved_quad(GLuint texture, float tex_u_max, float tex_v_max, int eye, float quad_scale, int apply_eye_offset, int flip_v)
{
	const int segments = 32;
	const float radius = 8.0f * quad_scale;
	const float curve = 1.0f;
	const float height = 5.0f * quad_scale;
	const GLboolean prev_blend = glIsEnabled(GL_BLEND);
	const GLboolean prev_alpha = glIsEnabled(GL_ALPHA_TEST);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glUseProgram(0);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	{
		float l = 0.0f;
		float r = 0.0f;
		float b = 0.0f;
		float t = 0.0f;
		const float near_z = 0.5f;
		const float far_z = 10.0f;
		if (eye >= 0 && vr_openvr_eye_projection(eye, &l, &r, &b, &t))
			glFrustum(l * near_z, r * near_z, b * near_z, t * near_z, near_z, far_z);
		else
			glFrustum(-1.0, 1.0, -0.75, 0.75, near_z, far_z);
	}
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	if (apply_eye_offset)
		vr_openvr_apply_eye_modelview(eye);

	if (texture)
	{
		glBindTexture(GL_TEXTURE_2D, texture);
		glEnable(GL_TEXTURE_2D);
	}
	else
	{
		glDisable(GL_TEXTURE_2D);
	}
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	glBegin(GL_TRIANGLE_STRIP);
	for (int i = 0; i <= segments; i++)
	{
		float t = (float)i / (float)segments;
		float angle = (t - 0.5f) * curve;
		float x = sinf(angle) * radius;
		float z = -cosf(angle) * radius;
		float u = t * tex_u_max;
		const float v_top = flip_v ? 0.0f : tex_v_max;
		const float v_bottom = flip_v ? tex_v_max : 0.0f;
		glTexCoord2f(u, v_top);
		glVertex3f(x, height * 0.5f, z);
		glTexCoord2f(u, v_bottom);
		glVertex3f(x, -height * 0.5f, z);
	}
	glEnd();

	glDisable(GL_TEXTURE_2D);
	if (prev_blend)
		glEnable(GL_BLEND);
	if (prev_alpha)
		glEnable(GL_ALPHA_TEST);
}

static void vr_openvr_draw_flat_quad(GLuint texture, float tex_u_max, float tex_v_max, int eye, float quad_scale, int apply_eye_offset, int flip_v)
{
	const float width = 2.0f * quad_scale;
	const float height = 1.2f * quad_scale;
	const float depth = -2.0f;
	const GLboolean prev_blend = glIsEnabled(GL_BLEND);
	const GLboolean prev_alpha = glIsEnabled(GL_ALPHA_TEST);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glUseProgram(0);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	{
		float l = 0.0f;
		float r = 0.0f;
		float b = 0.0f;
		float t = 0.0f;
		const float near_z = 0.5f;
		const float far_z = 10.0f;
		if (eye >= 0 && vr_openvr_eye_projection(eye, &l, &r, &b, &t))
			glFrustum(l * near_z, r * near_z, b * near_z, t * near_z, near_z, far_z);
		else
			glFrustum(-1.0, 1.0, -0.75, 0.75, near_z, far_z);
	}
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	if (apply_eye_offset)
		vr_openvr_apply_eye_modelview(eye);

	if (texture)
	{
		glBindTexture(GL_TEXTURE_2D, texture);
		glEnable(GL_TEXTURE_2D);
	}
	else
	{
		glDisable(GL_TEXTURE_2D);
	}
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	glBegin(GL_TRIANGLE_STRIP);
	const float v_top = flip_v ? 0.0f : tex_v_max;
	const float v_bottom = flip_v ? tex_v_max : 0.0f;
	glTexCoord2f(0.0f, v_top);
	glVertex3f(-width * 0.5f, -height * 0.5f, depth);
	glTexCoord2f(0.0f, v_bottom);
	glVertex3f(-width * 0.5f, height * 0.5f, depth);
	glTexCoord2f(tex_u_max, v_top);
	glVertex3f(width * 0.5f, -height * 0.5f, depth);
	glTexCoord2f(tex_u_max, v_bottom);
	glVertex3f(width * 0.5f, height * 0.5f, depth);
	glEnd();

	glDisable(GL_TEXTURE_2D);
	if (prev_blend)
		glEnable(GL_BLEND);
	if (prev_alpha)
		glEnable(GL_ALPHA_TEST);
}

void vr_openvr_draw_menu_quad_for_eye(int eye, int curved, float scale, int mono_mode, int flip_v)
{
#ifdef USE_OPENVR
#ifdef OGL
	if (!vr_openvr_active() || !vr_gl_ready || !vr_menu_tex)
		return;

	glBindFramebuffer(GL_FRAMEBUFFER, vr_eye_fbo[eye]);
	glViewport(0, 0, vr_render_width, vr_render_height);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	if (curved)
		vr_openvr_draw_curved_quad(vr_menu_tex, 1.0f, 1.0f, eye, scale, !mono_mode, flip_v);
	else
		vr_openvr_draw_flat_quad(vr_menu_tex, 1.0f, 1.0f, eye, scale, !mono_mode, flip_v);
#else
	(void)eye;
	(void)curved;
	(void)scale;
	(void)mono_mode;
	(void)flip_v;
#endif
#else
	(void)eye;
	(void)curved;
	(void)scale;
	(void)mono_mode;
	(void)flip_v;
#endif
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
	int vr_w, vr_h;
	vr_openvr_render_size(&vr_w, &vr_h);
	grd_curscreen->sc_w = vr_w;
	grd_curscreen->sc_h = vr_h;
	gr_set_current_canvas(NULL);
	grd_curcanv->cv_bitmap.bm_w = vr_w;
	grd_curcanv->cv_bitmap.bm_h = vr_h;
	Game_screen_mode = SM(vr_w, vr_h);
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
	if (poses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
	{
		const vr::HmdMatrix34_t &mat = poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking;
		vr_head_orient.rvec.x = fl2f(mat.m[0][0]);
		vr_head_orient.rvec.y = fl2f(mat.m[0][1]);
		vr_head_orient.rvec.z = fl2f(mat.m[0][2]);
		vr_head_orient.uvec.x = fl2f(mat.m[1][0]);
		vr_head_orient.uvec.y = fl2f(mat.m[1][1]);
		vr_head_orient.uvec.z = fl2f(mat.m[1][2]);
		vr_head_orient.fvec.x = fl2f(mat.m[2][0]);
		vr_head_orient.fvec.y = fl2f(mat.m[2][1]);
		vr_head_orient.fvec.z = fl2f(mat.m[2][2]);
		vr_head_pos.x = fl2f(mat.m[0][3]);
		vr_head_pos.y = fl2f(mat.m[1][3]);
		vr_head_pos.z = -fl2f(mat.m[2][3]);
		vr_has_pose = true;
	}
	else
	{
		vr_has_pose = false;
	}
#endif
}

template <typename T>
static auto vr_openvr_reset_seated_zero_pose(T *system) -> decltype(system->ResetSeatedZeroPose(), void())
{
	system->ResetSeatedZeroPose();
}

static void vr_openvr_reset_seated_zero_pose(...)
{
}

void vr_openvr_recenter(void)
{
#ifdef USE_OPENVR
	if (!vr_openvr_active() || !vr_system)
		return;

	vr_openvr_reset_seated_zero_pose(vr_system);
	vr_head_orient = vmd_identity_matrix;
	vr_head_pos.x = 0;
	vr_head_pos.y = 0;
	vr_head_pos.z = 0;
	vr_has_pose = false;
#endif
}

fix vr_openvr_eye_offset(int eye)
{
#ifdef USE_OPENVR
	if (!vr_openvr_active() || !vr_system)
		return 0;

	vr::ETrackedPropertyError error = vr::TrackedProp_Success;
	float ipd_m = vr_system->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd,
		vr::Prop_UserIpdMeters_Float, &error);
	if (error != vr::TrackedProp_Success || ipd_m <= 0.0f)
		ipd_m = 0.064f;
	float offset_m = (eye == 0) ? -ipd_m * 0.5f : ipd_m * 0.5f;
	offset_m += (eye == 0) ? vr_eye_offset_adjust_m : -vr_eye_offset_adjust_m;
	return fl2f(offset_m);
#else
	(void)eye;
	return 0;
#endif
}

void vr_openvr_adjust_eye_offset(float delta_meters)
{
#ifdef USE_OPENVR
	vr_eye_offset_adjust_m += delta_meters;
#else
	(void)delta_meters;
#endif
}

int vr_openvr_eye_projection(int eye, float *left, float *right, float *bottom, float *top)
{
#ifdef USE_OPENVR
	if (!vr_openvr_active() || !vr_system)
		return 0;

	vr::Hmd_Eye vr_eye = (eye == 0) ? vr::Eye_Left : vr::Eye_Right;
	float l = 0.0f;
	float r = 0.0f;
	float t = 0.0f;
	float b = 0.0f;
	vr_system->GetProjectionRaw(vr_eye, &l, &r, &t, &b);
	if (left)
		*left = l;
	if (right)
		*right = r;
	if (bottom)
		*bottom = b;
	if (top)
		*top = t;
	return 1;
#else
	(void)eye;
	(void)left;
	(void)right;
	(void)bottom;
	(void)top;
	return 0;
#endif
}

int vr_openvr_head_pose(vms_matrix *orient, vms_vector *position)
{
#ifdef USE_OPENVR
	if (!vr_openvr_active() || !vr_has_pose)
		return 0;

	if (orient)
		*orient = vr_head_orient;
	if (position)
		*position = vr_head_pos;
	return 1;
#else
	(void)orient;
	(void)position;
	return 0;
#endif
}

int vr_openvr_current_eye(void)
{
#ifdef USE_OPENVR
	if (!vr_openvr_active())
		return -1;
	if (vr_current_eye < 0)
		return -1;
	return 1 - vr_current_eye;
#else
	return -1;
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
	vr_prev_screen_width = grd_curscreen->sc_w;
	vr_prev_screen_height = grd_curscreen->sc_h;
	vr_prev_canvas_bm_w = grd_curcanv->cv_bitmap.bm_w;
	vr_prev_canvas_bm_h = grd_curcanv->cv_bitmap.bm_h;
	vr_prev_canvas_bm_rowsize = grd_curcanv->cv_bitmap.bm_rowsize;
	vr_prev_last_width = last_width;
	vr_prev_last_height = last_height;
	glBindFramebuffer(GL_FRAMEBUFFER, vr_eye_fbo[eye]);
	glViewport(0, 0, vr_render_width, vr_render_height);
	grd_curscreen->sc_w = (int)vr_render_width;
	grd_curscreen->sc_h = (int)vr_render_height;
	grd_curcanv->cv_bitmap.bm_w = (int)vr_render_width;
	grd_curcanv->cv_bitmap.bm_h = (int)vr_render_height;
	grd_curcanv->cv_bitmap.bm_rowsize = (int)vr_render_width;
	last_width = vr_render_width;
	last_height = vr_render_height;
	vr_current_eye = eye;
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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
	grd_curscreen->sc_w = vr_prev_screen_width;
	grd_curscreen->sc_h = vr_prev_screen_height;
	grd_curcanv->cv_bitmap.bm_w = vr_prev_canvas_bm_w;
	grd_curcanv->cv_bitmap.bm_h = vr_prev_canvas_bm_h;
	grd_curcanv->cv_bitmap.bm_rowsize = vr_prev_canvas_bm_rowsize;
	last_width = vr_prev_last_width;
	last_height = vr_prev_last_height;

	vr_current_eye = -1;
#endif
#endif
}

void vr_openvr_submit_eyes(void)
{
#ifdef USE_OPENVR
#ifdef OGL
	if (!vr_openvr_active() || !vr_compositor || !vr_gl_ready)
		return;
//
	for (int eye = 0; eye < 2; eye++)
	{
		GLint prev_fbo = 0;
		GLint prev_viewport[4] = {0, 0, 0, 0};
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
		glGetIntegerv(GL_VIEWPORT, prev_viewport);
		glBindFramebuffer(GL_FRAMEBUFFER, vr_eye_fbo[eye]);
		glViewport(0, 0, (GLint)vr_render_width, (GLint)vr_render_height);
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
#ifdef OGLES
		glOrthof(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);
#else
		glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);
#endif
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		ogl_do_palfx();
		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
		glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
	}
//
	vr::Texture_t left = {(void *)(uintptr_t)vr_eye_color[0], vr::TextureType_OpenGL, vr::ColorSpace_Auto};
	vr::Texture_t right = {(void *)(uintptr_t)vr_eye_color[1], vr::TextureType_OpenGL, vr::ColorSpace_Auto};
	vr_compositor->Submit(vr::Eye_Left, &left);
	vr_compositor->Submit(vr::Eye_Right, &right);

	if (!vr_skip_mirror_blit)
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, vr_eye_fbo[0]);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		GLint blit_width = (GLint)vr_render_width;
		GLint blit_height = (GLint)vr_render_height;
		glBlitFramebuffer(0, 0, blit_width, blit_height, 0, 0, sdl_window_width, sdl_window_height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	}
#endif
#endif
}

static void vr_openvr_submit_mono_from_buffer(int curved, int read_front)
{
#ifdef USE_OPENVR
#ifdef OGL
	if (!vr_openvr_active() || !vr_gl_ready || !vr_compositor)
		return;

	vr_openvr_begin_frame();
	GLint prev_draw_fbo = 0;
	GLint prev_read_fbo = 0;
	GLint prev_read_buffer = GL_BACK;
	GLint prev_draw_buffer = GL_BACK;
	GLint prev_pack_alignment = 4;
	GLint prev_unpack_alignment = 4;
	GLboolean prev_scissor = GL_FALSE;
	GLint prev_scissor_box[4] = {0, 0, 0, 0};
	GLint double_buffer = 0;
#ifdef GL_DRAW_FRAMEBUFFER_BINDING
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw_fbo);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read_fbo);
#else
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_draw_fbo);
	prev_read_fbo = prev_draw_fbo;
#endif
	glGetIntegerv(GL_READ_BUFFER, &prev_read_buffer);
	glGetIntegerv(GL_DRAW_BUFFER, &prev_draw_buffer);   
	glGetIntegerv(GL_PACK_ALIGNMENT, &prev_pack_alignment);
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_unpack_alignment);
	glGetIntegerv(GL_DOUBLEBUFFER, &double_buffer);
	prev_scissor = glIsEnabled(GL_SCISSOR_TEST);
	glGetIntegerv(GL_SCISSOR_BOX, prev_scissor_box);
#ifdef GL_READ_FRAMEBUFFER
	glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prev_read_fbo);
#else
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_read_fbo);
#endif
	if (prev_read_fbo)
	{
		if (prev_read_buffer == GL_NONE)
			glReadBuffer(GL_COLOR_ATTACHMENT0);
		else if (prev_read_buffer == GL_BACK || prev_read_buffer == GL_FRONT)
			glReadBuffer(GL_COLOR_ATTACHMENT0);
		else
			glReadBuffer(prev_read_buffer);
	}
	else
	{
		GLint fallback = prev_draw_buffer;
		if (prev_read_buffer != GL_NONE)
			glReadBuffer(prev_read_buffer);
		else if (fallback != GL_NONE)
			glReadBuffer(fallback);
		else
			glReadBuffer(double_buffer ? GL_BACK : GL_FRONT);
	}
	glDisable(GL_SCISSOR_TEST);
	glFlush();
	glBindTexture(GL_TEXTURE_2D, vr_menu_tex);
	GLint copy_width = (GLint)vr_render_width;
	GLint copy_height = (GLint)vr_render_height;
	if (copy_width > grd_curscreen->sc_w)
		copy_width = grd_curscreen->sc_w;
	if (copy_height > grd_curscreen->sc_h)
		copy_height = grd_curscreen->sc_h;
	size_t copy_size = (size_t)copy_width * (size_t)copy_height * 4;
	if (copy_size > vr_menu_pixels_size)
	{
		uint8_t *new_pixels = (uint8_t *)realloc(vr_menu_pixels, copy_size);
		if (new_pixels)
		{
			vr_menu_pixels = new_pixels;
			vr_menu_pixels_size = copy_size;
		}
	}
	if (vr_menu_pixels && copy_size > 0)
	{
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glReadPixels(0, 0, copy_width, copy_height, GL_RGBA, GL_UNSIGNED_BYTE, vr_menu_pixels);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, copy_width, copy_height, GL_RGBA, GL_UNSIGNED_BYTE, vr_menu_pixels);
	}
	glPixelStorei(GL_PACK_ALIGNMENT, prev_pack_alignment);
	glPixelStorei(GL_UNPACK_ALIGNMENT, prev_unpack_alignment);
	glReadBuffer(prev_read_buffer);
	if (prev_scissor)
		glEnable(GL_SCISSOR_TEST);
	else
		glDisable(GL_SCISSOR_TEST);
	glScissor(prev_scissor_box[0], prev_scissor_box[1], prev_scissor_box[2], prev_scissor_box[3]);

	for (int eye = 0; eye < 2; eye++)
	{
		vr_openvr_draw_menu_quad_for_eye(eye, curved, 1.0f, 1, 1);
	}
    if (prev_read_fbo)
	{
#ifdef GL_READ_FRAMEBUFFER
		glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prev_read_fbo);
#else
		glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_read_fbo);
#endif
	}
#ifdef GL_DRAW_FRAMEBUFFER
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)prev_draw_fbo);
#else
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_draw_fbo);
#endif
	vr_skip_mirror_blit = true;
	vr_openvr_submit_eyes();
	vr_skip_mirror_blit = false;
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
	GLint prev_draw_fbo = 0;
	GLint prev_read_fbo = 0;
	GLint prev_read_buffer = GL_BACK;
	GLint prev_draw_buffer = GL_BACK;
	GLboolean prev_scissor = GL_FALSE;
	GLint prev_scissor_box[4] = {0, 0, 0, 0};
	GLint double_buffer = 0;
#ifdef GL_DRAW_FRAMEBUFFER_BINDING
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw_fbo);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read_fbo);
#else
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_draw_fbo);
	prev_read_fbo = prev_draw_fbo;
#endif
	glGetIntegerv(GL_READ_BUFFER, &prev_read_buffer);
	glGetIntegerv(GL_DRAW_BUFFER, &prev_draw_buffer);
	glGetIntegerv(GL_DOUBLEBUFFER, &double_buffer);
	prev_scissor = glIsEnabled(GL_SCISSOR_TEST);
	glGetIntegerv(GL_SCISSOR_BOX, prev_scissor_box);
#ifdef GL_READ_FRAMEBUFFER
	glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prev_read_fbo);
#else
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_read_fbo);
#endif
	if (prev_read_fbo)
	{
			if (prev_read_buffer == GL_NONE)
			glReadBuffer(GL_COLOR_ATTACHMENT0);
			else if (prev_read_buffer == GL_BACK || prev_read_buffer == GL_FRONT)
			glReadBuffer(GL_COLOR_ATTACHMENT0);
		    else
			glReadBuffer(prev_read_buffer);
	}
	else
	{
		GLint fallback = prev_draw_buffer;
		if (prev_read_buffer != GL_NONE)
			glReadBuffer(prev_read_buffer);
		else if (fallback != GL_NONE)
			glReadBuffer(fallback);
		else
			glReadBuffer(double_buffer ? GL_BACK : GL_FRONT);
	}
	glDisable(GL_SCISSOR_TEST);
	glFlush();
	glBindTexture(GL_TEXTURE_2D, vr_menu_tex);
	GLint copy_width = (GLint)vr_render_width;
	GLint copy_height = (GLint)vr_render_height;
	if (copy_width > grd_curscreen->sc_w)
		copy_width = grd_curscreen->sc_w;
	if (copy_height > grd_curscreen->sc_h)
		copy_height = grd_curscreen->sc_h;
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, copy_width, copy_height);
	glReadBuffer(prev_read_buffer);
	if (prev_scissor)
		glEnable(GL_SCISSOR_TEST);
	else
		glDisable(GL_SCISSOR_TEST);
	glScissor(prev_scissor_box[0], prev_scissor_box[1], prev_scissor_box[2], prev_scissor_box[3]);

	for (int eye = 0; eye < 2; eye++)
	{
		vr_openvr_draw_menu_quad_for_eye(eye, curved, 1.0f, 1, 1);
	}
	if (prev_read_fbo)
	{
#ifdef GL_READ_FRAMEBUFFER
		glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prev_read_fbo);
#else
		glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_read_fbo);
#endif
	}
#ifdef GL_DRAW_FRAMEBUFFER
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)prev_draw_fbo);
#else
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_draw_fbo);
#endif
	vr_skip_mirror_blit = true;
	vr_openvr_submit_eyes();
	vr_skip_mirror_blit = false;
#endif
#endif
}

void vr_openvr_submit_mono_from_frontbuffer(int curved)
{
	vr_openvr_submit_mono_from_buffer(curved, 1);
}

void vr_openvr_bind_menu_target(void)
{
#ifdef USE_OPENVR
#ifdef OGL
	if (!vr_openvr_active() || !vr_gl_ready || !vr_menu_fbo)
		return;
		
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &vr_prev_fbo);
	glGetIntegerv(GL_VIEWPORT, vr_prev_viewport);
	vr_prev_canvas_width = Canvas_width;
	vr_prev_canvas_height = Canvas_height;
	vr_prev_last_width = last_width;
	vr_prev_last_height = last_height;
	glBindFramebuffer(GL_FRAMEBUFFER, vr_menu_fbo);
	glViewport(0, 0, vr_render_width, vr_render_height);
	Canvas_width = (int)vr_render_width;
	Canvas_height = (int)vr_render_height;
	last_width = (int)vr_render_width;
	last_height = (int)vr_render_height;
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, 1.0, 1.0, 0.0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
#endif
#endif
}

void vr_openvr_unbind_menu_target(void)
{
#ifdef USE_OPENVR
#ifdef OGL
	if (!vr_openvr_active() || !vr_gl_ready)
		return;

	extern int last_width, last_height;
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)vr_prev_fbo);
	glViewport(vr_prev_viewport[0], vr_prev_viewport[1], vr_prev_viewport[2], vr_prev_viewport[3]);
	Canvas_width = vr_prev_canvas_width;
	Canvas_height = vr_prev_canvas_height;
	last_width = vr_prev_last_width;
	last_height = vr_prev_last_height;
#endif
#endif
}

void vr_openvr_submit_menu(int curved)
{
#ifdef USE_OPENVR
#ifdef OGL
	if (!vr_openvr_active() || !vr_gl_ready || !vr_menu_tex)
		return;

	vr_openvr_submit_mono_from_texture(vr_menu_tex, 1.0f, 1.0f, curved);
#endif
#endif
}

void vr_openvr_submit_mono_from_texture(unsigned int texture, float u, float v, int curved)
{
#ifdef USE_OPENVR
#ifdef OGL
	if (!vr_openvr_active() || !vr_gl_ready || !vr_compositor)
		return;

	vr_openvr_begin_frame();
	for (int eye = 0; eye < 2; eye++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, vr_eye_fbo[eye]);
		glViewport(0, 0, vr_render_width, vr_render_height);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		if (curved)
			vr_openvr_draw_curved_quad(texture, u, v, eye, 1.0f, 1, 0);
		else
			vr_openvr_draw_flat_quad(texture, u, v, eye, 1.0f, 1, 0);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	vr_skip_mirror_blit = true;
	vr_openvr_submit_eyes();
	vr_skip_mirror_blit = false;
#endif
#endif
}
