#include "edm.h"
#include "gpu.h"

struct edm *edm;

struct edf_line_stat {
    u32 i,row,col;
};

struct fgbg {
    struct rgba fg;
    struct rgba bg;
};

static inline struct fgbg edm_make_fgbg(struct rgba fg, struct rgba bg) {
    return (struct fgbg) {.fg = fg, .bg = bg};
}

static inline bool edf_will_wrap_w(struct editor_file *edf, u32 cc)
{
    return edf->view.ext.w < edf->view.ofs.x + gpu->cell.dim_px.w * cc;
}

static inline bool edf_will_wrap_h(struct editor_file *edf, u32 lc)
{
    return edf->view.ext.h < edf->view.ofs.y + gpu->cell.dim_px.h * lc;
}

internal void edf_newline(struct edf_line_stat *els)
{
    els->i += 1;
    els->row += 1;
    els->col = 0;
}

internal void edf_newcol(struct edf_line_stat *els)
{
    els->i += 1;
    els->col += 1;
}


internal void edf_virtual_newline(struct edf_line_stat *els)
{
    els->row += 1;
    els->col = 0;
}

#define EDM_ROW_PAD 0 /* padding between rows in pixels */
#define EDM_CSR_PAD 1 /* increase cursor size in y*/

static inline struct rect_u16 edm_make_char_rect(struct edf_line_stat els, struct offset_u16 view_ofs, char c) {
    log_error_if(is_whitechar(c), "This function should not be called with whitespace.");
    u32 i = char_to_glyph(c);
    struct rect_u16 r = {};
    r.ofs.x = (u16)(view_ofs.x + gpu->cell.dim_px.w * els.col + gpu->glyph[i].x);
    r.ofs.y = (u16)(view_ofs.y + (gpu->cell.dim_px.h + EDM_ROW_PAD) * (els.row+1) + gpu->glyph[i].y);
    r.ext.w = gpu->glyph[i].w;
    r.ext.h = gpu->glyph[i].h;
    return r;
}

static inline struct rect_u16 edm_make_cursor_rect(struct edf_line_stat els, struct offset_u16 view_ofs) {
    struct rect_u16 r = {};
    r.ofs.x = (u16)(view_ofs.x + gpu->cell.dim_px.w * els.col);
    r.ofs.y = (u16)(view_ofs.y + (gpu->cell.dim_px.h + EDM_ROW_PAD) * (els.row+1) + (s16)gpu->cell.y_ofs - EDM_CSR_PAD);
    r.ext.w = gpu->cell.dim_px.w;
    r.ext.h = gpu->cell.dim_px.h + EDM_CSR_PAD;
    return r;
}

static inline struct fgbg edm_char_col(char c)
{
    log_error_if(is_whitechar(c), "This function should not be called with whitespace.");
    struct fgbg r = edm_make_fgbg(FG_COL, BG_COL);
    r.bg.a = char_to_glyph(c);
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

internal void edf_draw_file(struct editor_file *edf)
{
    charset_t wsnl = create_charset();
    charset_add(&wsnl, ' ');
    charset_add(&wsnl, '\n');
    
    struct edf_line_stat els = {};
    
    for(els.i = edf->view_pos; els.i < edf->fb.size; edf_newcol(&els)) {
        if (els.i == edf->cursor_pos) {
            struct rect_u16 c = edm_make_cursor_rect(els, edf->view.ofs);
            struct rgba fg={},bg={};
            
            rgb_copy(&fg, &CSR_FG);
            rgb_copy(&bg, &CSR_BG);
            gpu_db_add(c, fg, bg);
        }
        
        if (is_whitechar(edf->fb.data[els.i])) {
            do {
                while(els.i < edf->fb.size && edf->fb.data[els.i] == '\n')
                    edf_newline(&els);
                while(edf->fb.data[els.i] == ' ') {
                    if (edf_will_wrap_w(edf, els.col))
                        edf_newline(&els);
                    else
                        edf_newcol(&els);
                }
            } while(is_whitechar(edf->fb.data[els.i]));
            
            if (edf->flags & EDF_WRAP) {
                u32 l = strfindcharset(create_string(edf->fb.data + els.i, edf->fb.size - els.i), wsnl);
                if (edf_will_wrap_w(edf, els.col + l))
                    edf_virtual_newline(&els);
            }
        }
        
        if (edf_will_wrap_w(edf, els.col)) {
            if (edf->flags & EDF_WRAP) {
                edf_virtual_newline(&els);
            } else {
                u32 inc = strfindchar(create_string(edf->fb.data + els.i, edf->fb.size - els.i), '\n');
                if (inc == Max_u32)
                    break;
                els.i += inc-1;
                continue;
            }
        }
        if (edf_will_wrap_h(edf, els.row))
            break;
        
        struct fgbg col = edm_char_col(edf->fb.data[els.i]);
        struct rect_u16 r = edm_make_char_rect(els, edf->view.ofs, edf->fb.data[els.i]);
        
        if (els.i == edf->cursor_pos) {
            rgb_copy(&col.fg, &CSR_FG);
            rgb_copy(&col.bg, &CSR_BG);
        }
        
        gpu_db_add(r, col.fg, col.bg);
    }
}

def_edm_update(edm_update)
{
    struct string data = CLSTR(edm_test_string);
    
    struct editor_file edf = {};
    edf.flags = EDF_SHWN|EDF_WRAP;
    edf.view_pos = 0;
    edf.cursor_pos = 57;
    //edf.cursor_pos = 19;
    
    u16 x = 100;
    u16 y = 50;
    edf.view.ofs.x = x;
    edf.view.ofs.y = y;
    edf.view.ext.w = win->dim.w - x;
    edf.view.ext.h = win->dim.h - y;
    
    edf.fb = data;
    edf.uri = (struct string) {};
    
    edf_draw_file(&edf);
    
    return 0;
}