// sqlshared.cpp -- Shared code between the main server and the sqlslave.
//
// $Id: sqlshared.cpp 8 2006-09-05 01:55:58Z brazilofmux $
//
#include "autoconf.h"
#include "config.h"

#ifdef QUERY_SLAVE
#include "sqlshared.h"

void sqlfoo(void)
{
    return;
}

#endif // QUERY_SLAVE
