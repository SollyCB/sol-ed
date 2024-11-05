#include "edm.h"
#include "gpu.h"

struct edm *edm;

struct fgbg {
    struct rgba fg;
    struct rgba bg;
};

static inline struct fgbg edm_make_fgbg(struct rgba fg, struct rgba bg) {
    return (struct fgbg) {.fg = fg, .bg = bg};
}

#define EDM_ROW_PAD 1 /* padding between rows in pixels */

// @Todo Maybe I want to tightly pack chars rather than having a
// consistent cell width? Idk what is most readable.
static inline struct rect_u16 edm_make_char_rect(u16 col, u16 row, struct offset_u16 view_ofs, char c) {
    struct rect_u16 r = {};
    r.ofs.x = view_ofs.x;
    r.ofs.y = view_ofs.y;
    if (is_whitechar(c)) {
        r.ofs.x += gpu->cell.dim_px.w * col;
        r.ofs.y += (gpu->cell.dim_px.h + EDM_ROW_PAD) * row;
        r.ext.w = gpu->cell.dim_px.w;
        r.ext.h = gpu->cell.dim_px.h;
        return r;
    } else {
        u32 i = char_to_glyph(c);
        r.ofs.x += gpu->cell.dim_px.w * col + gpu->glyph[i].x;
        r.ofs.y += (gpu->cell.dim_px.h + EDM_ROW_PAD) * (row+1) + gpu->glyph[i].y;
        r.ext.w = gpu->glyph[i].w;
        r.ext.h = gpu->glyph[i].h;
        return r;
    }
}

static inline struct fgbg edm_char_col(char c)
{
    struct fgbg r = edm_make_fgbg(FG_COL, BG_COL);
    if (is_whitechar(c)) {
        r.fg.a = 0;
    } else {
        r.bg.a = char_to_glyph(c);
    }
    return r;
}

def_create_edm(create_edm)
{
    if (create_edf_array(16, &get_thread_alloc(MT).persist, &edm->edf)) {
        log_error("Failed to create edm array");
        return -1;
    }
    return 0;
}

char edm_test_string[] = "\
Line 1: This is a line of text\n\
Line 2: This is another line, but it has more letters\n\
Line 3: This line has the line above appended to it in order to test long strings: This is another line, but it has more letters\n\
Line 4: Now these lines will repeat, take a look!\n\
\n\
Line 1: This is a line of text\n\
Line 2: This is another line, but it has more letters\n\
Line 3: This line has the line above appended to it in order to test long strings: This is another line, but it has more letters\n\
Line 4: Now these lines will repeat, take a look!\n\
\n\
Line 1: This is a line of text\n\
Line 2: This is another line, but it has more letters\n\
Line 3: This line has the line above appended to it in order to test long strings: This is another line, but it has more letters\n\
Line 4: Now these lines will repeat, take a look!\n\
\n\
Line 1: This is a line of text\n\
Line 2: This is another line, but it has more letters\n\
Line 3: This line has the line above appended to it in order to test long strings: This is another line, but it has more letters\n\
Line 4: Now these lines will repeat, take a look!\n\
\n\
Line 1: This is a line of text\n\
Line 2: This is another line, but it has more letters\n\
Line 3: This line has the line above appended to it in order to test long strings: This is another line, but it has more letters\n\
Line 4: Now these lines will repeat, take a look!\n\
\n\
Line 1: This is a line of text\n\
Line 2: This is another line, but it has more letters\n\
Line 3: This line has the line above appended to it in order to test long strings: This is another line, but it has more letters\n\
Line 4: Now these lines will repeat, take a look!\n\
";

def_edm_update(edm_update)
{
    struct string data = CLSTR(edm_test_string);
    
    struct editor_file edf = {};
    edf.flags = EDF_SHWN;
    edf.view_pos = 0;
    edf.cursor_pos = 0;
    
    u16 x = 100;
    u16 y = 50;
    edf.view.ofs.x = x;
    edf.view.ofs.y = y;
    edf.view.ext.w = win->dim.w - x;
    edf.view.ext.h = win->dim.h - y;
    
    edf.fb = data;
    edf.uri = (struct string) {};
    
    u16 lc=0,cc=0;
    for(u32 i=0; i < edf.fb.size; ++i) {
        
        struct fgbg col = edm_char_col(edf.fb.data[i]);
        struct rect_u16 r = edm_make_char_rect(cc, lc, edf.view.ofs, edf.fb.data[i]);
        
        if (edf.fb.data[i] == '\n') {
            lc += 1;
            cc = 0;
        } else {
            cc += 1;
        }
        
        if (r.ofs.x + r.ext.w < edf.view.ext.w &&
            r.ofs.y + r.ext.h < edf.view.ext.h)
        {
            if (i == edf.cursor_pos) {
                gpu_db_add(r, CSR_FG, CSR_BG);
                gpu_db_add(r, CSR_FG, CSR_BG);
            } else {
                gpu_db_add(r, col.fg, col.bg);
            }
        }
        
    }
    return 0;
}