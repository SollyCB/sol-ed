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

// @Todo Maybe I want to tightly pack chars rather than having a consistent cell width? Idk what is most readable.
static inline struct rect_u16 edm_make_char_rect(u16 col, u16 row, char c) {
    struct rect_u16 r = {};
    if (is_whitechar(c)) {
        r.ofs.x = gpu->cell.dim_px.w * col;
        r.ofs.y = gpu->cell.dim_px.h * row;
        r.ext.w = gpu->cell.dim_px.w;
        r.ext.h = gpu->cell.dim_px.h;
        return r;
    } else {
        u32 i = char_to_glyph(c);
        r.ofs.x = gpu->cell.dim_px.w * col + gpu->glyph[i].x;
        r.ofs.y = gpu->cell.dim_px.h * (row+1) + gpu->glyph[i].y;
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

def_edm_update(edm_update)
{
    struct string str = STR("Um, Hello World!\nThis is a really long string to test whether the renderpass will have problems now because I think that it should not");
    //struct string str = STR("the quick brown fox");
    
    u16 lc=0,cc=0;
    for(u32 i=0; i < str.size; ++i) {
        
        struct fgbg col = edm_char_col(str.data[i]);
        struct rect_u16 r = edm_make_char_rect(cc, lc, str.data[i]);
        
        if (str.data[i] == '\n') {
            lc += 1;
            cc = 0;
        } else {
            cc += 1;
        }
        
        if (r.ofs.x + r.ext.w < win->dim.w && r.ofs.y + r.ext.h < win->dim.h)
            gpu_db_add(r, col.fg, col.bg);
        
    }
    
    
    return 0;
}