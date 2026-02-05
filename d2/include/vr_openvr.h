#ifndef VR_OPENVR_H
#define VR_OPENVR_H

#include "pstypes.h"
#include "fix.h"
#include "vecmat.h"

#ifdef __cplusplus
extern "C" {
#endif

void vr_openvr_init(void);
void vr_openvr_init_gl(void);
void vr_openvr_shutdown(void);
int vr_openvr_active(void);
void vr_openvr_begin_frame(void);
void vr_openvr_recenter(void);
fix vr_openvr_eye_offset(int eye);
void vr_openvr_adjust_eye_offset(float delta_meters);
int vr_openvr_eye_projection(int eye, float *left, float *right, float *bottom, float *top);
int vr_openvr_head_pose(vms_matrix *orient, vms_vector *position);
int vr_openvr_current_eye(void);
void vr_openvr_render_size(int *width, int *height);
void vr_openvr_bind_eye(int eye);
void vr_openvr_unbind_eye(void);
void vr_openvr_submit_eyes(void);
void vr_openvr_submit_mono_from_screen(int curved);
void vr_openvr_submit_mono_from_frontbuffer(int curved);
void vr_openvr_bind_menu_target(void);
void vr_openvr_unbind_menu_target(void);
void vr_openvr_submit_menu(int curved);
void vr_openvr_submit_mono_from_texture(unsigned int texture, float u, float v, int curved);
void vr_openvr_draw_menu_quad_for_eye(int eye, int curved, float scale, int mono_mode, int flip_v);

#ifdef __cplusplus
}
#endif

#endif
