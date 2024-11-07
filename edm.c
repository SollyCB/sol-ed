#include "edm.h"
#include "gpu.h"

struct edm *edm;

struct edf_line_stat {
    u64 i;
    u16 ofs,row,col;
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

static inline int edf_next_line(struct editor_file *edf, struct edf_line_stat *els)
{
    u32 inc = strfindchar(create_string(edf->fb.data + els->i, edf->fb.size - els->i), '\n');
    if (inc == Max_u32)
        return -1;
    els->i += inc;
    return 0;
}

static inline u32 edf_word_len(struct editor_file *edf, struct edf_line_stat els)
{
    charset_t wsnl = create_charset();
    charset_add(&wsnl, ' ');
    charset_add(&wsnl, '\n');
    u32 l = strfindcharset(create_string(edf->fb.data + els.i, edf->fb.size - els.i), wsnl);
    return l == Max_u32 ? (u32)(edf->fb.size - els.i) : l;
}

internal void edf_newline(struct edf_line_stat *els)
{
    els->i += 1 + els->ofs;
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

static inline void edf_draw_cursor(struct editor_file *edf, struct edf_line_stat els)
{
    struct rect_u16 c = edm_make_cursor_rect(els, edf->view.ofs);
    struct rgba fg={},bg={};
    
    rgb_copy(&fg, &CSR_FG);
    rgb_copy(&bg, &CSR_BG);
    gpu_db_add(c, fg, bg);
}

static inline void edf_maybe_draw_cursor(struct editor_file *edf, struct edf_line_stat els)
{
    if (els.i != edf->cursor_pos)
        return;
    edf_draw_cursor(edf, els);
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
    struct edf_line_stat els = {};
    
    u64 view_pos = 0;
    {
        u16 i;
        for(i = edf->view_pos.x; i < edf->cursor_pos; ++i) {
            if (edf->fb.data[edf->cursor_pos - i] == '\n') {
                i -= 1;
                break;
            }
        }
        els.ofs = i - edf->view_pos.x;
        view_pos += els.ofs;
    }
    
    for(els.i = view_pos; els.i < edf->fb.size; edf_newcol(&els)) {
        main_loop_start: // goto label
        
        if (is_whitechar(edf->fb.data[els.i])) {
            do {
                // newline
                u16 cnt = 0;
                while(els.i < edf->fb.size && edf->fb.data[els.i] == '\n') {
                    edf_maybe_draw_cursor(edf, els);
                    els.i += 1;
                    cnt += 1;
                }
                if (cnt) {
                    els.row += cnt;
                    els.i += els.ofs;
                    els.col = 0;
                }
                
                // space
                while(edf->fb.data[els.i] == ' ') {
                    if (edf_will_wrap_w(edf, els.col)) {
                        if (edf->flags & EDF_WRAP) {
                            edf_virtual_newline(&els);
                        } else {
                            if (edf_next_line(edf, &els))
                                goto main_loop_end;
                            goto main_loop_start;
                        }
                    } else {
                        edf_maybe_draw_cursor(edf, els);
                        edf_newcol(&els);
                    }
                }
            } while(is_whitechar(edf->fb.data[els.i]));
            
            if (edf->flags & EDF_WRAP) {
                u32 l = edf_word_len(edf, els);
                if (edf_will_wrap_w(edf, els.col + l))
                    edf_virtual_newline(&els);
            }
        }
        
        if (edf_will_wrap_w(edf, els.col)) {
            if (edf->flags & EDF_WRAP) {
                edf_virtual_newline(&els);
            } else {
                if (edf_next_line(edf, &els))
                    goto main_loop_end;
                goto main_loop_start;
            }
        }
        if (edf_will_wrap_h(edf, els.row))
            break;
        
        struct fgbg col = edm_char_col(edf->fb.data[els.i]);
        struct rect_u16 r = edm_make_char_rect(els, edf->view.ofs, edf->fb.data[els.i]);
        
        if (els.i == edf->cursor_pos) {
            edf_draw_cursor(edf, els);
            rgb_copy(&col.fg, &CSR_FG);
            rgb_copy(&col.bg, &CSR_BG);
        }
        
        gpu_db_add(r, col.fg, col.bg);
    }
    main_loop_end: // goto label
    return;
}

def_edm_update(edm_update)
{
    struct string data = CLSTR(edm_test_string);
    
    struct editor_file edf = {};
    edf.flags = EDF_SHWN|EDF_WRAP;
    edf.cursor_pos = 57;
    
    u16 x = 100;
    u16 y = 50;
    edf.view.ofs.x = x;
    edf.view.ofs.y = y;
    edf.view.ext.w = win->dim.w - x;
    edf.view.ext.h = win->dim.h - y;
    
    edf.fb = data;
    edf.uri = (struct string) {};
    
    u16 i;
    for(i=0; i < edf.cursor_pos; ++i) {
        if (edf.fb.data[edf.cursor_pos - i] == '\n') {
            i -= 1;
            break;
        }
    }
    edf.view_pos.x = i - 4;
    edf.view_pos.y = 0;
    
    edf_draw_file(&edf);
    
    return 0;
}