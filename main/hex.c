#include <string.h>

int hexdigit_to_num(unsigned char c)
{
    if (c >= '0' && c <= '9' )
        return c - '0';
    else if( c >= 'a' && c <= 'f' )
        return c + 10 - 'a';
    else if( c >= 'A' && c <= 'F' )
        return c + 10 - 'A';
    else
        return -1;
}

/*
 * Convert a hex string to bytes.
 * Return 0 on success, -1 on error.
 */
int unhexify(unsigned char *buf)
{
    unsigned len = strlen((char *) buf);
    int i;

    if (len % 2 != 0) return -1;

    for (i=0; i < len; i += 2 )
    {
        unsigned char c;
        int num;

        num = hexdigit_to_num(buf[i]);
        if (num < 0) return -1;
        c = num << 4;

        num = hexdigit_to_num(buf[i+1]);
        if (num < 0) return -1;
        c |= num;

        buf[i/2] = c;
    }

    return 0;
}
