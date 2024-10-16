#include "../solh/sol.h"

#include "prg.h"
#include "win.h"
#include "gpu.h"

int main() {
    
    create_prg();
    
    while(!(win.flags & WIN_CLOSE)) {
        win_poll();
        struct keyboard_input ki;
        while(win_kb_next(&ki)) {
            char c = win_key_to_char(ki);
            if (ki.mod & RELEASE) continue;
            
            if (c > 0) {
                println("got input %c", c);
            } else {
                println("input is not a text char");
            }
        }
    }
    
    return 0;
}