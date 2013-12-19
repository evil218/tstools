/* vim: set tabstop=8 shiftwidth=8:
 * funx: to test zconv module
 */

#include <stdio.h>
#include <stdint.h> /* for uint?_t, etc */

#include "zconv.h"

int main()
{
        char str[256] = "\0";
        uint8_t *p;
        uint8_t dat[16];
        int i, j;

        for(j = 2; j < 16; j++) {
                p = dat;
                for(i = 0; i < 16; i++) {
                        *p++ = (j << 4) | i;
                }
                latin_utf8((const uint8_t *)dat, str, 16, CODING_DVB6937_P);
                fprintf(stdout, "%s\n", str);
        }
}
