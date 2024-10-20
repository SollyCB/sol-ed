#ifndef CHARS_H
#define CHARS_H

#include "shader.h"

#define CHT_SZ SH_SI_CNT
extern char cht[CHT_SZ]; // char table in char.c

enum chars_enum {
    CH_a = 'a' - '!',
    CH_b = 'b' - '!',
    CH_c = 'c' - '!',
    CH_d = 'd' - '!',
    CH_e = 'e' - '!',
    CH_f = 'f' - '!',
    CH_g = 'g' - '!',
    CH_h = 'h' - '!',
    CH_i = 'i' - '!',
    CH_j = 'j' - '!',
    CH_k = 'k' - '!',
    CH_l = 'l' - '!',
    CH_m = 'm' - '!',
    CH_n = 'n' - '!',
    CH_o = 'o' - '!',
    CH_p = 'p' - '!',
    CH_q = 'q' - '!',
    CH_r = 'r' - '!',
    CH_s = 's' - '!',
    CH_t = 't' - '!',
    CH_u = 'u' - '!',
    CH_v = 'v' - '!',
    CH_w = 'w' - '!',
    CH_x = 'x' - '!',
    CH_y = 'y' - '!',
    CH_z = 'z' - '!',
    
    CH_A = 'A' - '!',
    CH_B = 'B' - '!',
    CH_C = 'C' - '!',
    CH_D = 'D' - '!',
    CH_E = 'E' - '!',
    CH_F = 'F' - '!',
    CH_G = 'G' - '!',
    CH_H = 'H' - '!',
    CH_I = 'I' - '!',
    CH_J = 'J' - '!',
    CH_K = 'K' - '!',
    CH_L = 'L' - '!',
    CH_M = 'M' - '!',
    CH_N = 'N' - '!',
    CH_O = 'O' - '!',
    CH_P = 'P' - '!',
    CH_Q = 'Q' - '!',
    CH_R = 'R' - '!',
    CH_S = 'S' - '!',
    CH_T = 'T' - '!',
    CH_U = 'U' - '!',
    CH_V = 'V' - '!',
    CH_W = 'W' - '!',
    CH_X = 'X' - '!',
    CH_Y = 'Y' - '!',
    CH_Z = 'Z' - '!',
    
    CH_1 = '1' - '!',
    CH_2 = '2' - '!',
    CH_3 = '3' - '!',
    CH_4 = '4' - '!',
    CH_5 = '5' - '!',
    CH_6 = '6' - '!',
    CH_7 = '7' - '!',
    CH_8 = '8' - '!',
    CH_9 = '9' - '!',
    CH_0 = '0' - '!',
    
    CH_EXC = '!' - '!',
    CH_AT = '@' - '!',
    CH_POU = '#' - '!',
    CH_DOL = '$' - '!',
    CH_PRC = '%' - '!',
    CH_CAR = '^' - '!',
    CH_AMP = '&' - '!',
    CH_AST = '*' - '!',
    CH_LBRC = '(' - '!',
    CH_RBRC = ')' - '!',
    
    CH_MIN = '-' - '!',
    CH_EQ = '=' - '!',
    CH_LBRK = '[' - '!',
    CH_RBRK = ']' - '!',
    CH_BSL = '\\' - '!',
    CH_SCLN = ';' - '!',
    CH_APOS = '\'' - '!',
    CH_GRV = '`' - '!',
    CH_COM = ',' - '!',
    CH_PER = '.' - '!',
    CH_SL = '/' - '!',
    
    CH_UND = '_' - '!',
    CH_PLU = '+' - '!',
    CH_LCRL = '{' - '!',
    CH_RCRL = '}' - '!',
    CH_BAR = '|' - '!',
    CH_CLN = ':' - '!',
    CH_SPCH = '"' - '!',
    CH_TLD = '~' - '!',
    CH_LARR = '<' - '!',
    CH_RARR = '>' - '!',
    CH_QUES = '?' - '!',
};

#endif //CHARS_H