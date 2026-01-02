#ifndef VR_OPENVR_H
#define VR_OPENVR_H

#include "pstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

void vr_openvr_init(void);
void vr_openvr_init_gl(void);
void vr_openvr_shutdown(void);
int vr_openvr_active(void);
void vr_openvr_begin_frame(void);
fix vr_openvr_eye_offset(int eye);
void vr_openvr_bind_eye(int eye);
void vr_openvr_unbind_eye(void);
void vr_openvr_submit_eyes(void);
void vr_openvr_submit_mono_from_screen(void);

#ifdef __cplusplus
}
#endif

#endif
