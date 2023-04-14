#ifndef UNUSED_H
#define UNUSED_H

#define UNUSED(x) \
    do            \
    {             \
        (void)x;  \
    } while (0)

#endif