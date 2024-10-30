#ifndef EDM_H
#define EDM_H

#include "../solh/sol.h"

enum edf_flags {
    EDF_SHOWN = 0x01,
};

struct editor_file {
    u32 flags;
    u32 cursor_pos; // char that the cursor is on
    u32 view_pos; // char that is the top left corner of the view
    struct rect_u16 view; // pixel region on screen that the view is rendered to
    struct string data;
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
