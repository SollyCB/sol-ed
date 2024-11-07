#ifndef EDM_H
#define EDM_H

#include "../solh/sol.h"

enum edf_flags {
    EDF_SHWN = 0x01,
    EDF_TSHW = 0x02,
    EDF_THDE = 0x04,
    EDF_WRAP = 0x08,
};

struct editor_file {
    u32 flags;
    struct offset_u16 view_pos; // distance in cells to cursor from the top left corner of the view
    struct rect_u16 view; // pixel region on screen that the view is rendered to
    u64 cursor_pos; // char that the cursor is on
    struct string fb; // file buffer
    struct string uri;
};
def_typed_array(edf, struct editor_file)

struct edm {
    u32 active_file;
    edf_array_t edf;
};

#ifdef LIB
extern struct edm *edm;

#define def_create_edm(name) void name(void)
def_create_edm(create_edm);

#define def_edm_update(name) int edm_update(void)
def_edm_update(edm_update);
#endif // LIB

#endif // EDM_H
