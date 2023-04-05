/*
** $Id: fprintf.c 8944 2007-12-29 08:29:16Z xwyan $
**
** fprintf.c: implementation of own_fprintf function.
**
** Copyright (C) 2004 ~ 2007 Feynman Software.
**
** Current maintainer: Wei Yongming
**
** Create date: 2004-02-02 by Wei yongming
*/

#include <stddef.h>
#include <limits.h>

#include "common.h"

#if defined (_MGUSE_OWN_STDIO) && defined (_MGUSE_OWN_FPRINTF)

int own_fprintf (FILE *stream, const char *format, ... )
{
    int rc;
    va_list ap;

    if (stream == stdin || stream == stdout || stream == stderr) {
        stream = (FILE*) &__mg_def_dev;
    }

    va_start (ap, format);

    rc = own_vfnprintf (stream, INT_MAX, format, ap);

    va_end (ap);

    return rc;
}

#endif /* _MGUSE_OWN_STDIO */

