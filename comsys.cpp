/*! \file comsys.cpp
 *  Channel Communication System.
 *
 * $Id: comsys.cpp 3052 2007-12-30 06:36:27Z brazilofmux $
 *
 * The functions here manage channels, channel membership, the comsys.db, and
 * the interaction of players and other objects with channels.
 */

#include "copyright.h"
#include "autoconf.h"
#include "config.h"
#include "externs.h"

#include <sys/types.h>

#include "ansi.h"
#include "attrs.h"
#include "command.h"
#include "comsys.h"
#include "functions.h"
#include "interface.h"
#include "powers.h"

static int num_channels;
static comsys_t *comsys_table[NUM_COMSYS];

#define DFLT_MAX_LOG        0
#define MIN_RECALL_REQUEST  1
#define DFLT_RECALL_REQUEST 10
#define MAX_RECALL_REQUEST  200

// Return value is a static buffer provided by RemoveSetOfCharacters.
//
static char *RestrictTitleValue(char *pTitleRequest)
{
    // First, remove all '\r\n\t' from the string.
    //
    char *pNewTitle = RemoveSetOfCharacters(pTitleRequest, "\r\n\t");

    // Optimize/terminate any ANSI in the string.
    //
    char NewTitle_ANSI[MAX_TITLE_LEN+1];
    size_t nVisualWidth;
    size_t nLen = ANSI_TruncateToField(pNewTitle, sizeof(NewTitle_ANSI),
        NewTitle_ANSI, sizeof(NewTitle_ANSI), &nVisualWidth,
        ANSI_ENDGOAL_NORMAL);
    memcpy(pNewTitle, NewTitle_ANSI, nLen+1);
    return pNewTitle;
}

static void do_setcomtitlestatus(dbref player, struct channel *ch, bool status)
{
    struct comuser *user = select_user(ch,player);
    if (ch && user)
    {
        user->ComTitleStatus = status;
    }
}

static void do_setnewtitle(dbref player, struct channel *ch, char *pValidatedTitle)
{
    struct comuser *user = select_user(ch, player);

    if (ch && user)
    {
        if (user->title)
        {
            MEMFREE(user->title);
            user->title = NULL;
        }
        user->title = StringClone(pValidatedTitle);
    }
}

void load_comsys_V4(FILE *fp)
{
    char buffer[200];
    if (  fgets(buffer, sizeof(buffer), fp)
       && strcmp(buffer, "*** Begin CHANNELS ***\n") == 0)
    {
        load_channels_V4(fp);
    }
    else
    {
        Log.tinyprintf("Error: Couldn't find Begin CHANNELS." ENDLINE);
        return;
    }

    if (  fgets(buffer, sizeof(buffer), fp)
       && strcmp(buffer, "*** Begin COMSYS ***\n") == 0)
    {
        load_comsystem_V4(fp);
    }
    else
    {
        Log.tinyprintf("Error: Couldn't find Begin COMSYS." ENDLINE);
        return;
    }
}

void load_comsys_V0123(FILE *fp)
{
    char buffer[200];
    if (  fgets(buffer, sizeof(buffer), fp)
       && strcmp(buffer, "*** Begin CHANNELS ***\n") == 0)
    {
        load_channels_V0123(fp);
    }
    else
    {
        Log.tinyprintf("Error: Couldn't find Begin CHANNELS." ENDLINE);
        return;
    }

    if (  fgets(buffer, sizeof(buffer), fp)
       && strcmp(buffer, "*** Begin COMSYS ***\n") == 0)
    {
        load_comsystem_V0123(fp);
    }
    else
    {
        Log.tinyprintf("Error: Couldn't find Begin COMSYS." ENDLINE);
        return;
    }
}

void load_comsys(char *filename)
{
    int i;

    for (i = 0; i < NUM_COMSYS; i++)
    {
        comsys_table[i] = NULL;
    }

    FILE *fp;
    if (!mux_fopen(&fp, filename, "rb"))
    {
        Log.tinyprintf("Error: Couldn't find %s." ENDLINE, filename);
    }
    else
    {
        DebugTotalFiles++;
        Log.tinyprintf("LOADING: %s" ENDLINE, filename);
        int ch = getc(fp);
        if (EOF == ch)
        {
            Log.tinyprintf("Error: Couldn't read first byte.");
        }
        else
        {
            ungetc(ch, fp);
            if ('+' == ch)
            {
                // Version 4 or later.
                //
                char nbuf1[8];

                // Read the version number.
                //
                if (fgets(nbuf1, sizeof(nbuf1), fp))
                {
                    if (strncmp(nbuf1, "+V4", 3) == 0)
                    {
                        // Started v4 on 2007-MAR-13.
                        //
                        load_comsys_V4(fp);
                    }
                }
            }
            else
            {
                load_comsys_V0123(fp);
            }
        }

        if (fclose(fp) == 0)
        {
            DebugTotalFiles--;
        }
        Log.tinyprintf("LOADING: %s (done)" ENDLINE, filename);
    }
}

void save_comsys(char *filename)
{
    char buffer[500];

    mux_sprintf(buffer, sizeof(buffer), "%s.#", filename);
    FILE *fp;
    if (!mux_fopen(&fp, buffer, "wb"))
    {
        Log.tinyprintf("Unable to open %s for writing." ENDLINE, buffer);
        return;
    }
    DebugTotalFiles++;
    fprintf(fp, "*** Begin CHANNELS ***\n");
    save_channels(fp);

    fprintf(fp, "*** Begin COMSYS ***\n");
    save_comsystem(fp);

    if (fclose(fp) == 0)
    {
        DebugTotalFiles--;
    }
    ReplaceFile(buffer, filename);
}

// Aliases must be between 1 and 5 characters. No spaces. No ANSI.
//
char *MakeCanonicalComAlias
(
    const char *pAlias,
    size_t *nValidAlias,
    bool *bValidAlias
)
{
    static char Buffer[ALIAS_SIZE];
    *nValidAlias = 0;
    *bValidAlias = false;

    if (!pAlias)
    {
        return NULL;
    }
    size_t n = 0;
    while (pAlias[n])
    {
        if (  !mux_isprint(pAlias[n])
           || ' ' == pAlias[n])
        {
            return NULL;
        }
        n++;
    }

    if (n < 1)
    {
        return NULL;
    }
    else if (MAX_ALIAS_LEN < n)
    {
        n = MAX_ALIAS_LEN;
    }
    memcpy(Buffer, pAlias, n);
    Buffer[n] = '\0';
    *nValidAlias = n;
    *bValidAlias = true;
    return Buffer;
}

static bool ParseChannelLine(char *pBuffer, char *pAlias5, char **ppChannelName)
{
    // Fetch alias portion. We need to find the first space.
    //
    char *p = strchr(pBuffer, ' ');
    if (!p)
    {
        return false;
    }

    *p = '\0';
    bool bValidAlias;
    size_t  nValidAlias;
    char *pValidAlias = MakeCanonicalComAlias(pBuffer, &nValidAlias, &bValidAlias);
    if (!bValidAlias)
    {
        return false;
    }
    mux_strncpy(pAlias5, pValidAlias, ALIAS_SIZE-1);

    // Skip any leading space before the channel name.
    //
    p++;
    while (mux_isspace(*p))
    {
        p++;
    }

    if (*p == '\0')
    {
        return false;
    }

    // The rest of the line is the channel name.
    //
    *ppChannelName = StringClone(p);
    return true;
}

static bool ReadListOfNumbers(FILE *fp, int cnt, int anum[])
{
    char buffer[200];
    if (fgets(buffer, sizeof(buffer), fp))
    {
        char *p = buffer;
        for (int i = 0; i < cnt; i++)
        {
            if (  mux_isdigit(p[0])
               || (  '-' == p[0]
                  && mux_isdigit(p[1])))
            {
                anum[i] = mux_atol(p);
                do
                {
                    p++;
                } while (mux_isdigit(*p));

                if (' ' == *p)
                {
                    p++;
                }
            }
            else
            {
                return false;
            }
        }

        if ('\n' == *p)
        {
            return true;
        }
    }
    return false;
}

void load_channels_V0123(FILE *fp)
{
    int i, j;
    int anum[2];
    char buffer[LBUF_SIZE];
    comsys_t *c;

    int np = 0;
    mux_assert(ReadListOfNumbers(fp, 1, &np));
    for (i = 0; i < np; i++)
    {
        c = create_new_comsys();
        c->who = 0;
        c->numchannels = 0;
        mux_assert(ReadListOfNumbers(fp, 2, anum));
        c->who = anum[0];
        c->numchannels = anum[1];
        c->maxchannels = c->numchannels;
        if (c->maxchannels > 0)
        {
            c->alias = (char *)MEMALLOC(c->maxchannels * ALIAS_SIZE);
            ISOUTOFMEMORY(c->alias);
            c->channels = (char **)MEMALLOC(sizeof(char *) * c->maxchannels);
            ISOUTOFMEMORY(c->channels);

            for (j = 0; j < c->numchannels; j++)
            {
                size_t n = GetLineTrunc(buffer, sizeof(buffer), fp);
                if (buffer[n-1] == '\n')
                {
                    // Get rid of trailing '\n'.
                    //
                    n--;
                    buffer[n] = '\0';
                }
                if (!ParseChannelLine(buffer, c->alias + j * ALIAS_SIZE, c->channels+j))
                {
                    c->numchannels--;
                    j--;
                }
            }
            sort_com_aliases(c);
        }
        else
        {
            c->alias = NULL;
            c->channels = NULL;
        }
        if (Good_obj(c->who))
        {
            add_comsys(c);
        }
        else
        {
            Log.tinyprintf("Invalid dbref %d." ENDLINE, c->who);
        }
        purge_comsystem();
    }
}

void load_channels_V4(FILE *fp)
{
    int i, j;
    int anum[2];
    char buffer[LBUF_SIZE];
    comsys_t *c;

    int np = 0;
    mux_assert(ReadListOfNumbers(fp, 1, &np));
    for (i = 0; i < np; i++)
    {
        c = create_new_comsys();
        c->who = 0;
        c->numchannels = 0;
        mux_assert(ReadListOfNumbers(fp, 2, anum));
        c->who = anum[0];
        c->numchannels = anum[1];
        c->maxchannels = c->numchannels;
        if (c->maxchannels > 0)
        {
            c->alias = (char *)MEMALLOC(c->maxchannels * ALIAS_SIZE);
            ISOUTOFMEMORY(c->alias);
            c->channels = (char **)MEMALLOC(sizeof(char *) * c->maxchannels);
            ISOUTOFMEMORY(c->channels);

            for (j = 0; j < c->numchannels; j++)
            {
                size_t n = GetLineTrunc(buffer, sizeof(buffer), fp);
                if (buffer[n-1] == '\n')
                {
                    // Get rid of trailing '\n'.
                    //
                    n--;
                    buffer[n] = '\0';
                }

                // Convert entire line including color codes.
                //
                char *pBuffer = (char *)convert_color((UTF8 *)buffer);
                pBuffer = ConvertToLatin((UTF8 *)pBuffer);

                if (!ParseChannelLine(pBuffer, c->alias + j * ALIAS_SIZE, c->channels+j))
                {
                    c->numchannels--;
                    j--;
                }
            }
            sort_com_aliases(c);
        }
        else
        {
            c->alias = NULL;
            c->channels = NULL;
        }
        if (Good_obj(c->who))
        {
            add_comsys(c);
        }
        else
        {
            Log.tinyprintf("Invalid dbref %d." ENDLINE, c->who);
        }
        purge_comsystem();
    }
}

void purge_comsystem(void)
{
#ifdef ABORT_PURGE_COMSYS
    return;
#endif // ABORT_PURGE_COMSYS

    comsys_t *c;
    comsys_t *d;
    int i;
    for (i = 0; i < NUM_COMSYS; i++)
    {
        c = comsys_table[i];
        while (c)
        {
            d = c;
            c = c->next;
            if (d->numchannels == 0)
            {
                del_comsys(d->who);
                continue;
            }
            if (isPlayer(d->who))
            {
                continue;
            }
            if (  God(Owner(d->who))
               && Going(d->who))
            {
                del_comsys(d->who);
                continue;
            }
        }
    }
}

void save_channels(FILE *fp)
{
    purge_comsystem();

    comsys_t *c;
    int i, j;
    int np = 0;
    for (i = 0; i < NUM_COMSYS; i++)
    {
        c = comsys_table[i];
        while (c)
        {
            np++;
            c = c->next;
        }
    }

    fprintf(fp, "%d\n", np);
    for (i = 0; i < NUM_COMSYS; i++)
    {
        c = comsys_table[i];
        while (c)
        {
            fprintf(fp, "%d %d\n", c->who, c->numchannels);
            for (j = 0; j < c->numchannels; j++)
            {
                fprintf(fp, "%s %s\n", c->alias + j * ALIAS_SIZE, c->channels[j]);
            }
            c = c->next;
        }
    }
}

comsys_t *create_new_comsys(void)
{
    comsys_t *c = (comsys_t *)MEMALLOC(sizeof(comsys_t));
    if (c)
    {
        c->who         = NOTHING;
        c->numchannels = 0;
        c->maxchannels = 0;
        c->alias       = NULL;
        c->channels    = NULL;
        c->next        = NULL;
    }
    else
    {
        ISOUTOFMEMORY(c);
    }
    return c;
}

static comsys_t *get_comsys(dbref which)
{
    if (which < 0)
    {
        return NULL;
    }

    comsys_t *c = comsys_table[which % NUM_COMSYS];

    while (c && (c->who != which))
        c = c->next;

    if (!c)
    {
        c = create_new_comsys();
        c->who = which;
        add_comsys(c);
    }
    return c;
}

void add_comsys(comsys_t *c)
{
    if (c->who < 0 || c->who >= mudstate.db_top)
    {
        Log.tinyprintf("add_comsys: dbref %d out of range [0, %d)" ENDLINE, c->who, mudstate.db_top);
        return;
    }

    c->next = comsys_table[c->who % NUM_COMSYS];
    comsys_table[c->who % NUM_COMSYS] = c;
}

void del_comsys(dbref who)
{
    if (who < 0 || who >= mudstate.db_top)
    {
        Log.tinyprintf("del_comsys: dbref %d out of range [0, %d)" ENDLINE, who, mudstate.db_top);
        return;
    }

    comsys_t *c = comsys_table[who % NUM_COMSYS];

    if (c == NULL)
    {
        return;
    }

    if (c->who == who)
    {
        comsys_table[who % NUM_COMSYS] = c->next;
        destroy_comsys(c);
        return;
    }
    comsys_t *last = c;
    c = c->next;
    while (c)
    {
        if (c->who == who)
        {
            last->next = c->next;
            destroy_comsys(c);
            return;
        }
        last = c;
        c = c->next;
    }
}

void destroy_comsys(comsys_t *c)
{
    int i;

    if (c->alias)
    {
        MEMFREE(c->alias);
        c->alias = NULL;
    }
    for (i = 0; i < c->numchannels; i++)
    {
        MEMFREE(c->channels[i]);
        c->channels[i] = NULL;
    }
    if (c->channels)
    {
        MEMFREE(c->channels);
        c->channels = NULL;
    }
    MEMFREE(c);
    c = NULL;
}

void sort_com_aliases(comsys_t *c)
{
    int i;
    char buffer[10];
    char *s;
    bool cont = true;

    while (cont)
    {
        cont = false;
        for (i = 0; i < c->numchannels - 1; i++)
        {
            if (strcmp(c->alias + i * ALIAS_SIZE, c->alias + (i + 1) * ALIAS_SIZE) > 0)
            {
                mux_strncpy(buffer, c->alias + i * ALIAS_SIZE, sizeof(buffer)-1);
                mux_strncpy(c->alias + i * ALIAS_SIZE, c->alias + (i + 1) * ALIAS_SIZE, ALIAS_SIZE-1);
                mux_strncpy(c->alias + (i + 1) * ALIAS_SIZE, buffer, ALIAS_SIZE-1);
                s = c->channels[i];
                c->channels[i] = c->channels[i + 1];
                c->channels[i + 1] = s;
                cont = true;
            }
        }
    }
}

static char *get_channel_from_alias(dbref player, char *alias)
{
    int first, last, current, dir;

    comsys_t *c = get_comsys(player);

    current = first = 0;
    last = c->numchannels - 1;
    dir = 1;

    while (dir && (first <= last))
    {
        current = (first + last) / 2;
        dir = strcmp(alias, c->alias + ALIAS_SIZE * current);
        if (dir < 0)
            last = current - 1;
        else
            first = current + 1;
    }

    if (!dir)
    {
        return c->channels[current];
    }
    else
    {
        return (char *)"";
    }
}

void load_comsystem_V0123(FILE *fp)
{
    int i, j;
    int ver = 0;
    struct channel *ch;
    char temp[LBUF_SIZE];

    num_channels = 0;

    int nc = 0;
    if (NULL == fgets(temp, sizeof(temp), fp))
    {
        return;
    }
    if (!strncmp(temp, "+V", 2))
    {
        // +V2 has colored headers
        //
        ver = mux_atol(temp + 2);
        if (ver < 1 || 3 < ver)
        {
            return;
        }

        mux_assert(ReadListOfNumbers(fp, 1, &nc));
    }
    else
    {
        nc = mux_atol(temp);
    }

    num_channels = nc;

    for (i = 0; i < nc; i++)
    {
        int anum[10];

        ch = (struct channel *)MEMALLOC(sizeof(struct channel));
        ISOUTOFMEMORY(ch);

        size_t nChannel = GetLineTrunc(temp, sizeof(temp), fp);
        if (nChannel > MAX_CHANNEL_LEN)
        {
            nChannel = MAX_CHANNEL_LEN;
        }
        if (temp[nChannel-1] == '\n')
        {
            // Get rid of trailing '\n'.
            //
            nChannel--;
        }
        memcpy(ch->name, temp, nChannel);
        ch->name[nChannel] = '\0';

        if (ver >= 2)
        {
            size_t nHeader = GetLineTrunc(temp, sizeof(temp), fp);
            if (nHeader > MAX_HEADER_LEN)
            {
                nHeader = MAX_HEADER_LEN;
            }
            if (temp[nHeader-1] == '\n')
            {
                nHeader--;
            }
            memcpy(ch->header, temp, nHeader);
            ch->header[nHeader] = '\0';
        }

        ch->on_users = NULL;

        hashaddLEN(ch->name, nChannel, ch, &mudstate.channel_htab);

        ch->type         = 127;
        ch->temp1        = 0;
        ch->temp2        = 0;
        ch->charge       = 0;
        ch->charge_who   = NOTHING;
        ch->amount_col   = 0;
        ch->num_messages = 0;
        ch->chan_obj     = NOTHING;

        if (ver >= 1)
        {
            mux_assert(ReadListOfNumbers(fp, 8, anum));
            ch->type         = anum[0];
            ch->temp1        = anum[1];
            ch->temp2        = anum[2];
            ch->charge       = anum[3];
            ch->charge_who   = anum[4];
            ch->amount_col   = anum[5];
            ch->num_messages = anum[6];
            ch->chan_obj     = anum[7];
        }
        else
        {
            mux_assert(ReadListOfNumbers(fp, 10, anum));
            ch->type         = anum[0];
            // anum[1] is not used.
            ch->temp1        = anum[2];
            ch->temp2        = anum[3];
            // anum[4] is not used.
            ch->charge       = anum[5];
            ch->charge_who   = anum[6];
            ch->amount_col   = anum[7];
            ch->num_messages = anum[8];
            ch->chan_obj     = anum[9];
        }

        if (ver <= 1)
        {
            // Build colored header if not +V2 or later db.
            //
            if (ch->type & CHANNEL_PUBLIC)
            {
                mux_sprintf(temp, sizeof(temp), "%s[%s%s%s%s%s]%s", ANSI_CYAN, ANSI_HILITE,
                    ANSI_BLUE, ch->name, ANSI_NORMAL, ANSI_CYAN, ANSI_NORMAL);
            }
            else
            {
                mux_sprintf(temp, sizeof(temp), "%s[%s%s%s%s%s]%s", ANSI_MAGENTA, ANSI_HILITE,
                    ANSI_RED, ch->name, ANSI_NORMAL, ANSI_MAGENTA,
                    ANSI_NORMAL);
            }
            size_t vwVisual;
            ANSI_TruncateToField(temp, MAX_HEADER_LEN+1, ch->header,
                MAX_HEADER_LEN+1, &vwVisual, ANSI_ENDGOAL_NORMAL);
        }

        ch->num_users = 0;
        mux_assert(ReadListOfNumbers(fp, 1, &(ch->num_users)));
        ch->max_users = ch->num_users;
        if (ch->num_users > 0)
        {
            ch->users = (struct comuser **)calloc(ch->max_users, sizeof(struct comuser *));
            ISOUTOFMEMORY(ch->users);

            int jAdded = 0;
            for (j = 0; j < ch->num_users; j++)
            {
                struct comuser t_user;
                memset(&t_user, 0, sizeof(t_user));

                t_user.who = NOTHING;
                t_user.bUserIsOn = false;
                t_user.ComTitleStatus = false;

                if (ver == 3)
                {
                    mux_assert(ReadListOfNumbers(fp, 3, anum));
                    t_user.who = anum[0];
                    t_user.bUserIsOn = (anum[1] ? true : false);
                    t_user.ComTitleStatus = (anum[2] ? true : false);
                }
                else
                {
                    t_user.ComTitleStatus = true;
                    if (ver)
                    {
                        mux_assert(ReadListOfNumbers(fp, 2, anum));
                        t_user.who = anum[0];
                        t_user.bUserIsOn = (anum[1] ? true : false);
                    }
                    else
                    {
                        mux_assert(ReadListOfNumbers(fp, 4, anum));
                        t_user.who = anum[0];
                        // anum[1] is not used.
                        // anum[2] is not used.
                        t_user.bUserIsOn = (anum[3] ? true : false);
                    }
                }

                // Read Comtitle.
                //
                size_t nTitle = GetLineTrunc(temp, sizeof(temp), fp);
                char *pTitle = temp;

                if (!Good_dbref(t_user.who))
                {
                    Log.tinyprintf("load_comsystem: dbref %d out of range [0, %d)." ENDLINE, t_user.who, mudstate.db_top);
                }
                else if (isGarbage(t_user.who))
                {
                    Log.tinyprintf("load_comsystem: dbref is GARBAGE." ENDLINE, t_user.who);
                }
                else
                {
                    // Validate comtitle
                    //
                    if (3 < nTitle && temp[0] == 't' && temp[1] == ':')
                    {
                        pTitle = temp+2;
                        nTitle -= 2;
                        if (pTitle[nTitle-1] == '\n')
                        {
                            // Get rid of trailing '\n'.
                            //
                            nTitle--;
                        }
                        if (nTitle <= 0 || MAX_TITLE_LEN < nTitle)
                        {
                            nTitle = 0;
                            pTitle = temp;
                        }
                    }
                    else
                    {
                        nTitle = 0;
                    }

                    struct comuser *user = (struct comuser *)MEMALLOC(sizeof(struct comuser));
                    ISOUTOFMEMORY(user);
                    memcpy(user, &t_user, sizeof(struct comuser));

                    user->title = StringCloneLen(pTitle, nTitle);
                    ch->users[jAdded++] = user;

                    if (  !(isPlayer(user->who))
                       && !(Going(user->who)
                       && (God(Owner(user->who)))))
                    {
                        do_joinchannel(user->who, ch);
                    }
                    user->on_next = ch->on_users;
                    ch->on_users = user;
                }
            }
            ch->num_users = jAdded;
            sort_users(ch);
        }
        else
        {
            ch->users = NULL;
        }
    }
}

// Version 4 start on 2007-MAR-17
//
//   -- Supports UTF-8 and ANSI as code-points.
//   -- Relies on a version number at the top of the file instead of within
//      this section.
//
void load_comsystem_V4(FILE *fp)
{
    int i, j;
    struct channel *ch;
    char temp[LBUF_SIZE];

    num_channels = 0;

    int nc = 0;
    if (NULL == fgets((char *)temp, sizeof(temp), fp))
    {
        return;
    }
    nc = mux_atol(temp);

    num_channels = nc;

    for (i = 0; i < nc; i++)
    {
        int anum[10];

        ch = (struct channel *)MEMALLOC(sizeof(struct channel));
        ISOUTOFMEMORY(ch);

        size_t nChannel = GetLineTrunc(temp, sizeof(temp), fp);
        if (nChannel > MAX_CHANNEL_LEN)
        {
            nChannel = MAX_CHANNEL_LEN;
        }

        if (temp[nChannel-1] == '\n')
        {
            // Get rid of trailing '\n'.
            //
            nChannel--;
            temp[nChannel] = '\0';
        }

        // Convert entire line including color codes.
        //
        char *pBuffer = (char *)convert_color((UTF8 *)temp);
        pBuffer = ConvertToLatin((UTF8 *)pBuffer);
        nChannel = strlen(pBuffer);
        if (MAX_CHANNEL_LEN < nChannel)
        {
            nChannel = MAX_CHANNEL_LEN;
        }

        memcpy(ch->name, pBuffer, nChannel);
        ch->name[nChannel] = '\0';

        size_t nHeader = GetLineTrunc(temp, sizeof(temp), fp);
        if (temp[nHeader-1] == '\n')
        {
            nHeader--;
            temp[nHeader] = '\0';
        }

        // Convert entire line including color codes.
        //
        pBuffer = (char *)convert_color((UTF8 *)temp);
        pBuffer = ConvertToLatin((UTF8 *)pBuffer);
        nHeader = strlen(pBuffer);
        if (MAX_HEADER_LEN < nHeader)
        {
            nHeader = MAX_HEADER_LEN;
        }

        memcpy(ch->header, pBuffer, nHeader);
        ch->header[nHeader] = '\0';

        ch->on_users = NULL;

        hashaddLEN(ch->name, nChannel, ch, &mudstate.channel_htab);

        ch->type         = 127;
        ch->temp1        = 0;
        ch->temp2        = 0;
        ch->charge       = 0;
        ch->charge_who   = NOTHING;
        ch->amount_col   = 0;
        ch->num_messages = 0;
        ch->chan_obj     = NOTHING;

        mux_assert(ReadListOfNumbers(fp, 8, anum));
        ch->type         = anum[0];
        ch->temp1        = anum[1];
        ch->temp2        = anum[2];
        ch->charge       = anum[3];
        ch->charge_who   = anum[4];
        ch->amount_col   = anum[5];
        ch->num_messages = anum[6];
        ch->chan_obj     = anum[7];

        ch->num_users = 0;
        mux_assert(ReadListOfNumbers(fp, 1, &(ch->num_users)));
        ch->max_users = ch->num_users;
        if (ch->num_users > 0)
        {
            ch->users = (struct comuser **)calloc(ch->max_users, sizeof(struct comuser *));
            ISOUTOFMEMORY(ch->users);

            int jAdded = 0;
            for (j = 0; j < ch->num_users; j++)
            {
                struct comuser t_user;
                memset(&t_user, 0, sizeof(t_user));

                t_user.who = NOTHING;
                t_user.bUserIsOn = false;
                t_user.ComTitleStatus = false;

                mux_assert(ReadListOfNumbers(fp, 3, anum));
                t_user.who = anum[0];
                t_user.bUserIsOn = (anum[1] ? true : false);
                t_user.ComTitleStatus = (anum[2] ? true : false);

                // Read Comtitle.
                //
                size_t nTitle = GetLineTrunc(temp, sizeof(temp), fp);

                // Convert entire line including color codes.
                //
                char *pTitle = (char *)convert_color((UTF8 *)temp);
                pTitle = ConvertToLatin((UTF8 *)pTitle);
                nTitle = strlen(pTitle);

                if (!Good_dbref(t_user.who))
                {
                    Log.tinyprintf("load_comsystem: dbref %d out of range [0, %d)." ENDLINE, t_user.who, mudstate.db_top);
                }
                else if (isGarbage(t_user.who))
                {
                    Log.tinyprintf("load_comsystem: dbref is GARBAGE." ENDLINE, t_user.who);
                }
                else
                {
                    // Validate comtitle.
                    //
                    if (  3 < nTitle
                       && pTitle[0] == 't'
                       && pTitle[1] == ':')
                    {
                        pTitle = pTitle+2;
                        nTitle -= 2;

                        if (pTitle[nTitle-1] == '\n')
                        {
                            // Get rid of trailing '\n'.
                            //
                            nTitle--;
                        }

                        if (  nTitle <= 0
                           || MAX_TITLE_LEN < nTitle)
                        {
                            nTitle = 0;
                            pTitle = pBuffer;
                        }
                    }
                    else
                    {
                        nTitle = 0;
                    }

                    struct comuser *user = (struct comuser *)MEMALLOC(sizeof(struct comuser));
                    ISOUTOFMEMORY(user);
                    memcpy(user, &t_user, sizeof(struct comuser));

                    user->title = StringCloneLen(pTitle, nTitle);
                    ch->users[jAdded++] = user;

                    if (  !(isPlayer(user->who))
                       && !(Going(user->who)
                       && (God(Owner(user->who)))))
                    {
                        do_joinchannel(user->who, ch);
                    }
                    user->on_next = ch->on_users;
                    ch->on_users = user;
                }
            }
            ch->num_users = jAdded;
            sort_users(ch);
        }
        else
        {
            ch->users = NULL;
        }
    }
}

void save_comsystem(FILE *fp)
{
    struct channel *ch;
    struct comuser *user;
    int j;

    fprintf(fp, "+V3\n");
    fprintf(fp, "%d\n", num_channels);
    for (ch = (struct channel *)hash_firstentry(&mudstate.channel_htab);
         ch;
         ch = (struct channel *)hash_nextentry(&mudstate.channel_htab))
    {
        fprintf(fp, "%s\n", ch->name);
        fprintf(fp, "%s\n", ch->header);

        fprintf(fp, "%d %d %d %d %d %d %d %d\n", ch->type, ch->temp1,
            ch->temp2, ch->charge, ch->charge_who, ch->amount_col,
            ch->num_messages, ch->chan_obj);

        // Count the number of 'valid' users to dump.
        //
        int nUsers = 0;
        for (j = 0; j < ch->num_users; j++)
        {
            user = ch->users[j];
            if (user->who >= 0 && user->who < mudstate.db_top)
            {
                nUsers++;
            }
        }

        fprintf(fp, "%d\n", nUsers);
        for (j = 0; j < ch->num_users; j++)
        {
            user = ch->users[j];
            if (user->who >= 0 && user->who < mudstate.db_top)
            {
                user = ch->users[j];
                fprintf(fp, "%d %d %d\n", user->who, user->bUserIsOn, user->ComTitleStatus);
                if (user->title[0] != '\0')
                {
                    fprintf(fp, "t:%s\n", user->title);
                }
                else
                {
                    fprintf(fp, "t:\n");
                }
            }
        }
    }
}

static void BuildChannelMessage
(
    bool bSpoof,
    const char *pHeader,
    struct comuser *user,
    char *pPose,
    char **messNormal,
    char **messNoComtitle
)
{
    // Allocate necessary buffers.
    //
    *messNormal = alloc_lbuf("BCM.messNormal");
    *messNoComtitle = NULL;
    if (!bSpoof)
    {
        *messNoComtitle = alloc_lbuf("BCM.messNoComtitle");
    }

    // Comtitle Check
    //
    bool hasComTitle = (user->title[0] != '\0');

    char *mnptr  = *messNormal;     // Message without comtitle removal
    char *mncptr = *messNoComtitle; // Message with comtitle removal

    safe_str(pHeader, *messNormal, &mnptr);
    safe_chr(' ', *messNormal, &mnptr);
    if (!bSpoof)
    {
        safe_str(pHeader, *messNoComtitle, &mncptr);
        safe_chr(' ', *messNoComtitle, &mncptr);
    }

    // Don't evaluate a title if there isn't one to parse or evaluation of
    // comtitles is disabled.
    // If they're set spoof, ComTitleStatus doesn't matter.
    if (hasComTitle && (user->ComTitleStatus || bSpoof))
    {
        if (mudconf.eval_comtitle)
        {
            // Evaluate the comtitle as code.
            //
            char TempToEval[LBUF_SIZE];
            mux_strncpy(TempToEval, user->title, LBUF_SIZE-1);
            char *q = TempToEval;
            mux_exec(*messNormal, &mnptr, user->who, user->who, user->who,
                EV_FCHECK | EV_EVAL | EV_TOP, &q, NULL, 0);
        }
        else
        {
            safe_str(user->title, *messNormal, &mnptr);
        }
        if (!bSpoof)
        {
            safe_chr(' ', *messNormal, &mnptr);
            safe_str(Moniker(user->who), *messNormal, &mnptr);
            safe_str(Moniker(user->who), *messNoComtitle, &mncptr);
        }
    }
    else
    {
        safe_str(Moniker(user->who), *messNormal, &mnptr);
        if (!bSpoof)
        {
            safe_str(Moniker(user->who), *messNoComtitle, &mncptr);
        }
    }

    char *saystring = NULL;
    char *newPose = NULL;

    switch(pPose[0])
    {
    case ':':
        pPose++;
        newPose = modSpeech(user->who, pPose, true, (char *)"channel/pose");
        if (newPose)
        {
            pPose = newPose;
        }
        safe_chr(' ', *messNormal, &mnptr);
        safe_str(pPose, *messNormal, &mnptr);
        if (!bSpoof)
        {
            safe_chr(' ', *messNoComtitle, &mncptr);
            safe_str(pPose, *messNoComtitle, &mncptr);
        }
        break;

    case ';':
        pPose++;
        newPose = modSpeech(user->who, pPose, true, (char *)"channel/pose");
        if (newPose)
        {
            pPose = newPose;
        }
        safe_str(pPose, *messNormal, &mnptr);
        if (!bSpoof)
        {
            safe_str(pPose, *messNoComtitle, &mncptr);
        }
        break;

    default:
        newPose = modSpeech(user->who, pPose, true, (char *)"channel");
        if (newPose)
        {
            pPose = newPose;
        }
        saystring = modSpeech(user->who, pPose, false, (char *)"channel");
        if (saystring)
        {
            safe_chr(' ', *messNormal, &mnptr);
            safe_str(saystring, *messNormal, &mnptr);
            safe_str(" \"", *messNormal, &mnptr);
        }
        else
        {
            safe_str(" says, \"", *messNormal, &mnptr);
        }
        safe_str(pPose, *messNormal, &mnptr);
        safe_chr('"', *messNormal, &mnptr);
        if (!bSpoof)
        {
            if (saystring)
            {
                safe_chr(' ', *messNoComtitle, &mncptr);
                safe_str(saystring, *messNoComtitle, &mncptr);
                safe_str(" \"", *messNoComtitle, &mncptr);
            }
            else
            {
                safe_str(" says, \"", *messNoComtitle, &mncptr);
            }
            safe_str(pPose, *messNoComtitle, &mncptr);
            safe_chr('"', *messNoComtitle, &mncptr);
        }
        break;
    }
    *mnptr = '\0';
    if (!bSpoof)
    {
        *mncptr = '\0';
    }
    if (newPose)
    {
        free_lbuf(newPose);
    }
    if (saystring)
    {
        free_lbuf(saystring);
    }
}

static void do_processcom(dbref player, char *arg1, char *arg2)
{
    if (!*arg2)
    {
        raw_notify(player, "No message.");
        return;
    }
    if (3500 < strlen(arg2))
    {
        arg2[3500] = '\0';
    }
    struct channel *ch = select_channel(arg1);
    if (!ch)
    {
        raw_notify(player, tprintf("Unknown channel %s.", arg1));
        return;
    }
    struct comuser *user = select_user(ch, player);
    if (!user)
    {
        raw_notify(player, "You are not listed as on that channel.  Delete this alias and readd.");
        return;
    }

#if !defined(FIRANMUX)
    if (  Gagged(player)
       && !Wizard(player))
    {
        raw_notify(player, "GAGGED players may not speak on channels.");
        return;
    }
#endif // FIRANMUX

    if (!strcmp(arg2, "on"))
    {
        do_joinchannel(player, ch);
    }
    else if (!strcmp(arg2, "off"))
    {
        do_leavechannel(player, ch);
    }
    else if (!user->bUserIsOn)
    {
        raw_notify(player, tprintf("You must be on %s to do that.", arg1));
        return;
    }
    else if (!strcmp(arg2, "who"))
    {
        do_comwho(player, ch);
    }
    else if (  !strncmp(arg2, "last", 4)
            && (  arg2[4] == '\0'
               || (  arg2[4] == ' '
                  && is_integer(arg2 + 5, NULL))))
    {
        // Parse optional number after the 'last' command.
        //
        int nRecall = DFLT_RECALL_REQUEST;
        if (arg2[4] == ' ')
        {
            nRecall = mux_atol(arg2 + 5);
        }
        do_comlast(player, ch, nRecall);
    }
    else if (!test_transmit_access(player, ch))
    {
        raw_notify(player, "That channel type cannot be transmitted on.");
        return;
    }
    else
    {
        if (!payfor(player, Guest(player) ? 0 : ch->charge))
        {
            notify(player, tprintf("You don't have enough %s.", mudconf.many_coins));
            return;
        }
        else
        {
            ch->amount_col += ch->charge;
            giveto(ch->charge_who, ch->charge);
        }

        // BuildChannelMessage allocates messNormal and messNoComtitle,
        // SendChannelMessage frees them.
        //
        char *messNormal;
        char *messNoComtitle;
        BuildChannelMessage((ch->type & CHANNEL_SPOOF) != 0, ch->header, user,
            arg2, &messNormal, &messNoComtitle);
        SendChannelMessage(player, ch, messNormal, messNoComtitle);
    }
}

void SendChannelMessage
(
    dbref executor,
    struct channel *ch,
    char *msgNormal,
    char *msgNoComtitle
)
{
    bool bSpoof = ((ch->type & CHANNEL_SPOOF) != 0);
    ch->num_messages++;

    struct comuser *user;
    for (user = ch->on_users; user; user = user->on_next)
    {
        if (  user->bUserIsOn
           && test_receive_access(user->who, ch))
        {
            if (  user->ComTitleStatus
               || bSpoof
               || msgNoComtitle == NULL)
            {
                notify_with_cause_ooc(user->who, executor, msgNormal);
            }
            else
            {
                notify_with_cause_ooc(user->who, executor, msgNoComtitle);
            }
        }
    }

    dbref obj = ch->chan_obj;
    if (Good_obj(obj))
    {
        dbref aowner;
        int aflags;
        int logmax = DFLT_MAX_LOG;
        char *maxbuf;
        ATTR *pattr = atr_str("MAX_LOG");
        if (  pattr
           && pattr->number)
        {
            maxbuf = atr_get(obj, pattr->number, &aowner, &aflags);
            logmax = mux_atol(maxbuf);
            free_lbuf(maxbuf);
        }
        if (logmax > 0)
        {
            if (logmax > MAX_RECALL_REQUEST)
            {
                logmax = MAX_RECALL_REQUEST;
                atr_add(ch->chan_obj, pattr->number, mux_ltoa_t(logmax), GOD,
                    AF_CONST|AF_NOPROG|AF_NOPARSE);
            }
            char *p = tprintf("HISTORY_%d", iMod(ch->num_messages, logmax));
            int atr = mkattr(GOD, p);
            if (0 < atr)
            {
                atr_add(ch->chan_obj, atr, msgNormal, GOD, AF_CONST|AF_NOPROG|AF_NOPARSE);
            }
        }
    }
    else if (ch->chan_obj != NOTHING)
    {
        ch->chan_obj = NOTHING;
    }

    // Since msgNormal and msgNoComTitle are no longer needed, free them here.
    //
    if (msgNormal)
    {
        free_lbuf(msgNormal);
    }
    if (  msgNoComtitle
       && msgNoComtitle != msgNormal)
    {
        free_lbuf(msgNoComtitle);
    }
}

void do_joinchannel(dbref player, struct channel *ch)
{
    struct comuser **cu;
    int i;

    struct comuser *user = select_user(ch, player);

    if (!user)
    {
        if (ch->num_users >= MAX_USERS_PER_CHANNEL)
        {
            raw_notify(player, tprintf("Too many people on channel %s already.", ch->name));
            return;
        }

        ch->num_users++;
        if (ch->num_users >= ch->max_users)
        {
            ch->max_users = ch->num_users + 10;
            cu = (struct comuser **)MEMALLOC(sizeof(struct comuser *) * ch->max_users);
            ISOUTOFMEMORY(cu);

            for (i = 0; i < (ch->num_users - 1); i++)
            {
                cu[i] = ch->users[i];
            }
            MEMFREE(ch->users);
            ch->users = cu;
        }
        user = (struct comuser *)MEMALLOC(sizeof(struct comuser));
        ISOUTOFMEMORY(user);

        for (i = ch->num_users - 1; i > 0 && ch->users[i - 1]->who > player; i--)
        {
            ch->users[i] = ch->users[i - 1];
        }
        ch->users[i] = user;

        user->who            = player;
        user->bUserIsOn      = true;
        user->ComTitleStatus = true;
        user->title          = StringClone("");

        // if (Connected(player))&&(isPlayer(player))
        //
        if (UNDEAD(player))
        {
            user->on_next = ch->on_users;
            ch->on_users  = user;
        }
    }
    else if (!user->bUserIsOn)
    {
        user->bUserIsOn = true;
    }
    else
    {
        raw_notify(player, tprintf("You are already on channel %s.", ch->name));
        return;
    }

    if (!Hidden(player))
    {
        char *messNormal, *messNoComtitle;
        BuildChannelMessage((ch->type & CHANNEL_SPOOF) != 0, ch->header, user,
            (char *)":has joined this channel.", &messNormal, &messNoComtitle);
        SendChannelMessage(player, ch, messNormal, messNoComtitle);
    }
}

void do_leavechannel(dbref player, struct channel *ch)
{
    struct comuser *user = select_user(ch, player);
    raw_notify(player, tprintf("You have left channel %s.", ch->name));
    if (  user->bUserIsOn
       && !Hidden(player))
    {
        char *messNormal, *messNoComtitle;
        BuildChannelMessage((ch->type & CHANNEL_SPOOF) != 0, ch->header, user,
            (char *)":has left this channel.", &messNormal, &messNoComtitle);
        SendChannelMessage(player, ch, messNormal, messNoComtitle);
    }
    user->bUserIsOn = false;
}

static void do_comwho_line
(
    dbref player,
    struct channel *ch,
    struct comuser *user
)
{
    char *msg;
    char *buff = NULL;

    if (user->title[0] != '\0')
    {
        // There is a comtitle
        //
        if (Staff(player))
        {
            buff = unparse_object(player, user->who, false);
            if (ch->type & CHANNEL_SPOOF)
            {
                msg = tprintf("%s as %s", buff, user->title);
            }
            else
            {
                msg = tprintf("%s as %s %s", buff, user->title, buff);
            }
        }
        else
        {
            if (ch->type & CHANNEL_SPOOF)
            {
                msg = user->title;
            }
            else
            {
                buff = unparse_object(player, user->who, false);
                msg = tprintf("%s %s", user->title, buff);
            }
        }
    }
    else
    {
        buff = unparse_object(player, user->who, false);
        msg = buff;
    }

    raw_notify(player, msg);
    if (buff)
    {
        free_lbuf(buff);
    }
}

void do_comwho(dbref player, struct channel *ch)
{
    struct comuser *user;

    raw_notify(player, "-- Players --");
    for (user = ch->on_users; user; user = user->on_next)
    {
        if (isPlayer(user->who))
        {
            if (  Connected(user->who)
               && (  !Hidden(user->who)
                  || Wizard_Who(player)
                  || See_Hidden(player)))
            {
                if (user->bUserIsOn)
                {
                    do_comwho_line(player, ch, user);
                }
            }
            else if (!Hidden(user->who))
            {
                do_comdisconnectchannel(user->who, ch->name);
            }
        }
    }
    raw_notify(player, "-- Objects --");
    for (user = ch->on_users; user; user = user->on_next)
    {
        if (!isPlayer(user->who))
        {
            if (  Going(user->who)
               && God(Owner(user->who)))
            {
                do_comdisconnectchannel(user->who, ch->name);
            }
            else if (user->bUserIsOn)
            {
                do_comwho_line(player, ch, user);
            }
        }
    }
    raw_notify(player, tprintf("-- %s --", ch->name));
}

void do_comlast(dbref player, struct channel *ch, int arg)
{
    if (!Good_obj(ch->chan_obj))
    {
        raw_notify(player, "Channel does not have an object.");
        return;
    }
    dbref aowner;
    int aflags;
    dbref obj = ch->chan_obj;
    int logmax = MAX_RECALL_REQUEST;
    ATTR *pattr = atr_str("MAX_LOG");
    if (  pattr
       && (atr_get_info(obj, pattr->number, &aowner, &aflags)))
    {
        char *maxbuf = atr_get(obj, pattr->number, &aowner, &aflags);
        logmax = mux_atol(maxbuf);
        free_lbuf(maxbuf);
    }
    if (logmax < 1)
    {
        raw_notify(player, "Channel does not log.");
        return;
    }
    if (arg < MIN_RECALL_REQUEST)
    {
        arg = MIN_RECALL_REQUEST;
    }
    if (arg > logmax)
    {
        arg = logmax;
    }

    char *message;
    int histnum = ch->num_messages - arg;

    raw_notify(player, "-- Begin Comsys Recall --");
    for (int count = 0; count < arg; count++)
    {
        histnum++;
        pattr = atr_str(tprintf("HISTORY_%d", iMod(histnum, logmax)));
        if (pattr)
        {
            message = atr_get(obj, pattr->number, &aowner, &aflags);
            raw_notify(player, message);
            free_lbuf(message);
        }
    }
    raw_notify(player, "-- End Comsys Recall --");
}

static bool do_chanlog(dbref player, char *channel, char *arg)
{
    UNUSED_PARAMETER(player);

    int value;
    if (  !*arg
       || !is_integer(arg, NULL)
       || (value = mux_atol(arg)) > MAX_RECALL_REQUEST)
    {
        return false;
    }
    if (value < 0)
    {
        value = 0;
    }
    struct channel *ch = select_channel(channel);
    if (!Good_obj(ch->chan_obj))
    {
        // No channel object has been set.
        //
        return false;
    }
    int atr = mkattr(GOD, "MAX_LOG");
    if (atr <= 0)
    {
        return false;
    }
    dbref aowner;
    int aflags;
    char *oldvalue = atr_get(ch->chan_obj, atr, &aowner, &aflags);
    if (oldvalue)
    {
        int oldnum = mux_atol(oldvalue);
        if (oldnum > value)
        {
            ATTR *hist;
            for (int count = 0; count <= oldnum; count++)
            {
                hist = atr_str(tprintf("HISTORY_%d", count));
                if (hist)
                {
                    atr_clr(ch->chan_obj, hist->number);
                }
            }
        }
        free_lbuf(oldvalue);
    }
    atr_add(ch->chan_obj, atr, mux_ltoa_t(value), GOD,
        AF_CONST|AF_NOPROG|AF_NOPARSE);
    return true;
}

struct channel *select_channel(char *channel)
{
    struct channel *cp = (struct channel *)hashfindLEN(channel,
        strlen(channel), &mudstate.channel_htab);
    return cp;
}

struct comuser *select_user(struct channel *ch, dbref player)
{
    if (!ch)
    {
        return NULL;
    }

    int first = 0;
    int last = ch->num_users - 1;
    int dir = 1;
    int current = 0;

    while (dir && (first <= last))
    {
        current = (first + last) / 2;
        if (ch->users[current] == NULL)
        {
            last--;
            continue;
        }
        if (ch->users[current]->who == player)
        {
            dir = 0;
        }
        else if (ch->users[current]->who < player)
        {
            dir = 1;
            first = current + 1;
        }
        else
        {
            dir = -1;
            last = current - 1;
        }
    }

    if (!dir)
    {
        return ch->users[current];
    }
    else
    {
        return NULL;
    }
}

#define MAX_ALIASES_PER_PLAYER 100

void do_addcom
(
    dbref executor,
    dbref caller,
    dbref enactor,
    int   key,
    int   nargs,
    char *arg1,
    char *channel
)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(key);
    UNUSED_PARAMETER(nargs);

    if (!mudconf.have_comsys)
    {
        raw_notify(executor, "Comsys disabled.");
        return;
    }
    bool bValidAlias;
    size_t  nValidAlias;
    char *pValidAlias = MakeCanonicalComAlias(arg1, &nValidAlias, &bValidAlias);
    if (!bValidAlias)
    {
        raw_notify(executor, "You need to specify a valid alias.");
        return;
    }
    if ('\0' == channel[0])
    {
        raw_notify(executor, "You need to specify a channel.");
        return;
    }

    int i, j, where;
    char *na;
    char **nc;
    struct channel *ch = select_channel(channel);
    char Buffer[MAX_CHANNEL_LEN+1];
    if (!ch)
    {
        size_t nVisualWidth;
        ANSI_TruncateToField(channel, sizeof(Buffer), Buffer, sizeof(Buffer), &nVisualWidth, ANSI_ENDGOAL_NORMAL);
        raw_notify(executor, tprintf("Channel %s does not exist yet.", Buffer));
        return;
    }
    if (!test_join_access(executor, ch))
    {
        raw_notify(executor, "Sorry, this channel type does not allow you to join.");
        return;
    }
    comsys_t *c = get_comsys(executor);
    if (c->numchannels >= MAX_ALIASES_PER_PLAYER)
    {
        raw_notify(executor, tprintf("Sorry, but you have reached the maximum number of aliases allowed."));
        return;
    }
    for (j = 0; j < c->numchannels && (strcmp(pValidAlias, c->alias + j * ALIAS_SIZE) > 0); j++)
    {
        ; // Nothing.
    }
    if (j < c->numchannels && !strcmp(pValidAlias, c->alias + j * ALIAS_SIZE))
    {
        char *p = tprintf("That alias is already in use for channel %s.", c->channels[j]);
        raw_notify(executor, p);
        return;
    }
    if (c->numchannels >= c->maxchannels)
    {
        c->maxchannels = c->numchannels + 10;

        na = (char *)MEMALLOC(ALIAS_SIZE * c->maxchannels);
        ISOUTOFMEMORY(na);
        nc = (char **)MEMALLOC(sizeof(char *) * c->maxchannels);
        ISOUTOFMEMORY(nc);

        for (i = 0; i < c->numchannels; i++)
        {
            mux_strncpy(na + i * ALIAS_SIZE, c->alias + i * ALIAS_SIZE, ALIAS_SIZE-1);
            nc[i] = c->channels[i];
        }
        if (c->alias)
        {
            MEMFREE(c->alias);
            c->alias = NULL;
        }
        if (c->channels)
        {
            MEMFREE(c->channels);
            c->channels = NULL;
        }
        c->alias = na;
        c->channels = nc;
    }
    where = c->numchannels++;
    for (i = where; i > j; i--)
    {
        mux_strncpy(c->alias + i * ALIAS_SIZE, c->alias + (i - 1) * ALIAS_SIZE, ALIAS_SIZE-1);
        c->channels[i] = c->channels[i - 1];
    }

    where = j;
    memcpy(c->alias + where * ALIAS_SIZE, pValidAlias, nValidAlias);
    *(c->alias + where * ALIAS_SIZE + nValidAlias) = '\0';
    c->channels[where] = StringClone(channel);

    if (!select_user(ch, executor))
    {
        do_joinchannel(executor, ch);
    }

    raw_notify(executor, tprintf("Channel %s added with alias %s.", channel, pValidAlias));
}

void do_delcom(dbref executor, dbref caller, dbref enactor, int eval, int key, char *arg1)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(eval);
    UNUSED_PARAMETER(key);

    if (!mudconf.have_comsys)
    {
        raw_notify(executor, "Comsys disabled.");
        return;
    }
    if (!arg1)
    {
        raw_notify(executor, "Need an alias to delete.");
        return;
    }
    comsys_t *c = get_comsys(executor);
    int i;

    for (i = 0; i < c->numchannels; i++)
    {
        if (!strcmp(arg1, c->alias + i * ALIAS_SIZE))
        {
            int itmp, found = 0;
            for (itmp = 0;itmp < c->numchannels; itmp++)
            {
                if (!strcmp(c->channels[itmp],c->channels[i]))
                {
                    found++;
                }
            }

            // If we found no other channels, delete it
            //
            if (found <= 1)
            {
                do_delcomchannel(executor, c->channels[i], false);
                raw_notify(executor, tprintf("Alias %s for channel %s deleted.",
                    arg1, c->channels[i]));
                MEMFREE(c->channels[i]);
            }
            else
            {
                raw_notify(executor, tprintf("Alias %s for channel %s deleted.",
                    arg1, c->channels[i]));
            }

            c->channels[i] = NULL;
            c->numchannels--;

            for (; i < c->numchannels; i++)
            {
                mux_strncpy(c->alias + i * ALIAS_SIZE, c->alias + (i + 1) * ALIAS_SIZE, ALIAS_SIZE-1);
                c->channels[i] = c->channels[i + 1];
            }
            return;
        }
    }
    raw_notify(executor, "Unable to find that alias.");
}

void do_delcomchannel(dbref player, char *channel, bool bQuiet)
{
    struct comuser *user;

    struct channel *ch = select_channel(channel);
    if (!ch)
    {
        raw_notify(player, tprintf("Unknown channel %s.", channel));
    }
    else
    {
        int i;
        int j = 0;
        for (i = 0; i < ch->num_users && !j; i++)
        {
            user = ch->users[i];
            if (user->who == player)
            {
                do_comdisconnectchannel(player, channel);
                if (!bQuiet)
                {
                    if (  user->bUserIsOn
                       && !Hidden(player))
                    {
                        char *messNormal, *messNoComtitle;
                        BuildChannelMessage((ch->type & CHANNEL_SPOOF) != 0,
                                            ch->header, user, (char *)":has left this channel.",
                                            &messNormal, &messNoComtitle);
                        SendChannelMessage(player, ch, messNormal, messNoComtitle);
                    }
                    raw_notify(player, tprintf("You have left channel %s.", channel));
                }

                if (user->title)
                {
                    MEMFREE(user->title);
                    user->title = NULL;
                }
                MEMFREE(user);
                user = NULL;
                j = 1;
            }
        }

        if (j)
        {
            ch->num_users--;
            for (i--; i < ch->num_users; i++)
            {
                ch->users[i] = ch->users[i + 1];
            }
        }
    }
}

void do_createchannel(dbref executor, dbref caller, dbref enactor, int eval, int key, char *channel)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(eval);
    UNUSED_PARAMETER(key);

    if ('\0' == channel[0])
    {
        raw_notify(executor, "You must specify a channel to create.");
        return;
    }

    if (!Comm_All(executor))
    {
        raw_notify(executor, NOPERM_MESSAGE);
        return;
    }

    struct channel *newchannel = (struct channel *)MEMALLOC(sizeof(struct channel));
    if (NULL == newchannel)
    {
        raw_notify(executor, "Out of memory.");
        return;
    }

    size_t vwChannel;
    size_t nNameNoANSI;
    char *pNameNoANSI;
    char Buffer[MAX_HEADER_LEN];
    size_t nChannel = ANSI_TruncateToField(channel, sizeof(Buffer),
        Buffer, sizeof(Buffer), &vwChannel, ANSI_ENDGOAL_NORMAL);
    if (nChannel == vwChannel)
    {
        // The channel name does not contain ANSI, so first, we add some to
        // get the header.
        //
        const size_t nMax = MAX_HEADER_LEN - (sizeof(ANSI_HILITE)-1)
                          - (sizeof(ANSI_NORMAL)-1) - 2;
        if (nChannel > nMax)
        {
            nChannel = nMax;
        }
        Buffer[nChannel] = '\0';
        mux_sprintf(newchannel->header, sizeof(newchannel->header),
            "%s[%s]%s", ANSI_HILITE, Buffer, ANSI_NORMAL);

        // Then, we use the non-ANSI part for the name.
        //
        nNameNoANSI = nChannel;
        pNameNoANSI = Buffer;
    }
    else
    {
        // The given channel name does contain ANSI.
        //
        memcpy(newchannel->header, Buffer, nChannel+1);
        pNameNoANSI = strip_ansi(Buffer, &nNameNoANSI);
    }

    if (nNameNoANSI > MAX_CHANNEL_LEN)
    {
        nNameNoANSI = MAX_CHANNEL_LEN;
    }

    memcpy(newchannel->name, pNameNoANSI, nNameNoANSI);
    newchannel->name[nNameNoANSI] = '\0';

    if (select_channel(newchannel->name))
    {
        raw_notify(executor, tprintf("Channel %s already exists.", newchannel->name));
        MEMFREE(newchannel);
        return;
    }

    newchannel->type = 127;
    newchannel->temp1 = 0;
    newchannel->temp2 = 0;
    newchannel->charge = 0;
    newchannel->charge_who = executor;
    newchannel->amount_col = 0;
    newchannel->num_users = 0;
    newchannel->max_users = 0;
    newchannel->users = NULL;
    newchannel->on_users = NULL;
    newchannel->chan_obj = NOTHING;
    newchannel->num_messages = 0;

    num_channels++;

    hashaddLEN(newchannel->name, strlen(newchannel->name), newchannel, &mudstate.channel_htab);

    // Report the channel creation using non-ANSI name.
    //
    raw_notify(executor, tprintf("Channel %s created.", newchannel->name));
}

void do_destroychannel(dbref executor, dbref caller, dbref enactor, int eval, int key, char *channel)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(eval);
    UNUSED_PARAMETER(key);

    struct channel *ch;
    int j;

    if (!mudconf.have_comsys)
    {
        raw_notify(executor, "Comsys disabled.");
        return;
    }
    ch = (struct channel *)hashfindLEN(channel, strlen(channel), &mudstate.channel_htab);

    if (!ch)
    {
        raw_notify(executor, tprintf("Could not find channel %s.", channel));
        return;
    }
    else if (  !Comm_All(executor)
            && !Controls(executor, ch->charge_who))
    {
        raw_notify(executor, NOPERM_MESSAGE);
        return;
    }
    num_channels--;
    hashdeleteLEN(channel, strlen(channel), &mudstate.channel_htab);

    for (j = 0; j < ch->num_users; j++)
    {
        MEMFREE(ch->users[j]);
        ch->users[j] = NULL;
    }
    MEMFREE(ch->users);
    ch->users = NULL;
    MEMFREE(ch);
    ch = NULL;
    raw_notify(executor, tprintf("Channel %s destroyed.", channel));
}

#if 0
void do_cleanupchannels(void)
{
    struct channel *ch;
    for (ch = (struct channel *)hash_firstentry(&mudstate.channel_htab);
         ch; ch = (struct channel *)hash_nextentry(&mudstate.channel_htab))
    {
        struct comuser *user, *prevuser = NULL;
        for (user = ch->on_users; user; )
        {
            if (isPlayer(user->who))
            {
                if (!test_join_access(user->who, ch))
                //if (!Connected(user->who))
                {
                    // Go looking for user in the array.
                    //
                    bool bFound = false;
                    int iPos;
                    for (iPos = 0; iPos < ch->num_users && !bFound; iPos++)
                    {
                        if (ch->users[iPos] == user)
                        {
                            bFound = true;
                        }
                    }

                    if (bFound)
                    {
                        // Remove user from the array.
                        //
                        ch->num_users--;
                        for (iPos--; iPos < ch->num_users; iPos++)
                        {
                            ch->users[iPos] = ch->users[iPos+1];
                        }

                        // Save user pointer for later reporting and freeing.
                        //
                        struct comuser *cuVictim = user;

                        // Unlink user from the list, and decide who to look at next.
                        //
                        if (prevuser)
                        {
                            prevuser->on_next = user->on_next;
                        }
                        else
                        {
                            ch->on_users = user->on_next;
                        }
                        user = user->on_next;

                        // Reporting
                        //
                        if (!Hidden(cuVictim->who))
                        {
                            char *mess = StartBuildChannelMessage(cuVictim->who,
                                (ch->type & CHANNEL_SPOOF) != 0, ch->header, cuVictim->title,
                                Moniker(cuVictim->who), ":is booted off the channel by the system.");
                            do_comsend(ch, mess);
                            EndBuildChannelMessage(mess);
                        }
                        raw_notify(cuVictim->who, tprintf("The system has booted you off channel %s.", ch->name));

                        // Freeing
                        //
                        if (cuVictim->title)
                        {
                            MEMFREE(cuVictim->title);
                            cuVictim->title = NULL;
                        }
                        MEMFREE(cuVictim);
                        cuVictim = NULL;

                        continue;
                    }
                }
            }

            prevuser = user;
            user = user->on_next;
        }
    }
}
#endif

static void do_listchannels(dbref player)
{
    struct channel *ch;
    char temp[LBUF_SIZE];

    bool perm = Comm_All(player);
    if (!perm)
    {
        raw_notify(player, "Warning: Only public channels and your channels will be shown.");
    }
    raw_notify(player, "*** Channel      --Flags--    Obj     Own   Charge  Balance  Users   Messages");

    for (ch = (struct channel *)hash_firstentry(&mudstate.channel_htab);
         ch; ch = (struct channel *)hash_nextentry(&mudstate.channel_htab))
    {
        if (  perm
           || (ch->type & CHANNEL_PUBLIC)
           || Controls(player, ch->charge_who))
        {
            mux_sprintf(temp, sizeof(temp), "%c%c%c %-13.13s %c%c%c/%c%c%c %7d %7d %8d %8d %6d %10d",
                (ch->type & CHANNEL_PUBLIC) ? 'P' : '-',
                (ch->type & CHANNEL_LOUD) ? 'L' : '-',
                (ch->type & CHANNEL_SPOOF) ? 'S' : '-',
                ch->name,
                (ch->type & CHANNEL_PLAYER_JOIN) ? 'J' : '-',
                (ch->type & CHANNEL_PLAYER_TRANSMIT) ? 'X' : '-',
                (ch->type & CHANNEL_PLAYER_RECEIVE) ? 'R' : '-',
                (ch->type & CHANNEL_OBJECT_JOIN) ? 'j' : '-',
                (ch->type & CHANNEL_OBJECT_TRANSMIT) ? 'x' : '-',
                (ch->type & CHANNEL_OBJECT_RECEIVE) ? 'r' : '-',
                (ch->chan_obj != NOTHING) ? ch->chan_obj : -1,
                ch->charge_who, ch->charge, ch->amount_col, ch->num_users, ch->num_messages);
            raw_notify(player, temp);
        }
    }
    raw_notify(player, "-- End of list of Channels --");
}

void do_comtitle
(
    dbref executor,
    dbref caller,
    dbref enactor,
    int   key,
    int   nargs,
    char *arg1,
    char *arg2
)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(nargs);

    if (!mudconf.have_comsys)
    {
        raw_notify(executor, "Comsys disabled.");
        return;
    }
    if (!*arg1)
    {
        raw_notify(executor, "Need an alias to do comtitle.");
        return;
    }

    char channel[MAX_CHANNEL_LEN+1];
    mux_strncpy(channel, get_channel_from_alias(executor, arg1), MAX_CHANNEL_LEN);

    if (channel[0] == '\0')
    {
        raw_notify(executor, "Unknown alias.");
        return;
    }
    struct channel *ch = select_channel(channel);
    if (ch)
    {
        if (select_user(ch, executor))
        {
            if (key == COMTITLE_OFF)
            {
                if ((ch->type & CHANNEL_SPOOF) == 0)
                {
                    raw_notify(executor, tprintf("Comtitles are now off for channel %s", channel));
                    do_setcomtitlestatus(executor, ch, false);
                }
                else
                {
                    raw_notify(executor, "You can not turn off comtitles on that channel.");
                }
            }
            else if (key == COMTITLE_ON)
            {
                raw_notify(executor, tprintf("Comtitles are now on for channel %s", channel));
                do_setcomtitlestatus(executor, ch, true);
            }
            else
            {
                char *pValidatedTitleValue = RestrictTitleValue(arg2);
                do_setnewtitle(executor, ch, pValidatedTitleValue);
                raw_notify(executor, tprintf("Title set to '%s' on channel %s.",
                    pValidatedTitleValue, channel));
            }
        }
    }
    else
    {
        raw_notify(executor, "Illegal comsys alias, please delete.");
    }
}

void do_comlist
(
    dbref executor,
    dbref caller,
    dbref enactor,
    int   eval,
    int key,
    char* pattern
)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(eval);
    UNUSED_PARAMETER(key);

    if (!mudconf.have_comsys)
    {
        raw_notify(executor, "Comsys disabled.");
        return;
    }

    bool bWild;
    if (  NULL != pattern
       && '\0' != *pattern)
    {
        bWild = true;
    }
    else
    {
        bWild = false;
    }

    raw_notify(executor, "Alias     Channel            Status   Title");

    comsys_t *c = get_comsys(executor);
    int i;
    for (i = 0; i < c->numchannels; i++)
    {
        struct comuser *user = select_user(select_channel(c->channels[i]), executor);
        if (user)
        {
            if (  !bWild
               || quick_wild(pattern,c->channels[i]))
            {
                char *p =
                    tprintf("%-9.9s %-18.18s %s %s %s",
                        c->alias + i * ALIAS_SIZE,
                        c->channels[i],
                        (user->bUserIsOn ? "on " : "off"),
                        (user->ComTitleStatus ? "con " : "coff"),
                        user->title);
                raw_notify(executor, p);
            }
        }
        else
        {
            raw_notify(executor, tprintf("Bad Comsys Alias: %s for Channel: %s", c->alias + i * ALIAS_SIZE, c->channels[i]));
        }
    }
    raw_notify(executor, "-- End of comlist --");
}

void do_channelnuke(dbref player)
{
    struct channel *ch;
    int j;

    for (ch = (struct channel *)hash_firstentry(&mudstate.channel_htab);
         ch; ch = (struct channel *)hash_nextentry(&mudstate.channel_htab))
    {
        if (player == ch->charge_who)
        {
            num_channels--;
            hashdeleteLEN(ch->name, strlen(ch->name), &mudstate.channel_htab);

            if (NULL != ch->users)
            {
                for (j = 0; j < ch->num_users; j++)
                {
                    MEMFREE(ch->users[j]);
                    ch->users[j] = NULL;
                }
                MEMFREE(ch->users);
                ch->users = NULL;
            }
            MEMFREE(ch);
            ch = NULL;
        }
    }
}

void do_clearcom(dbref executor, dbref caller, dbref enactor, int unused2)
{
    UNUSED_PARAMETER(unused2);

    if (!mudconf.have_comsys)
    {
        raw_notify(executor, "Comsys disabled.");
        return;
    }
    comsys_t *c = get_comsys(executor);

    int i;
    for (i = (c->numchannels) - 1; i > -1; --i)
    {
        do_delcom(executor, caller, enactor, 0, 0, c->alias + i * ALIAS_SIZE);
    }
}

void do_allcom(dbref executor, dbref caller, dbref enactor, int eval, int key, char *arg1)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(key);
    UNUSED_PARAMETER(eval);

    if (!mudconf.have_comsys)
    {
        raw_notify(executor, "Comsys disabled.");
        return;
    }
    if (  strcmp(arg1, "who") != 0
       && strcmp(arg1, "on")  != 0
       && strcmp(arg1, "off") != 0)
    {
        raw_notify(executor, "Only options available are: on, off and who.");
        return;
    }

    comsys_t *c = get_comsys(executor);
    int i;
    for (i = 0; i < c->numchannels; i++)
    {
        do_processcom(executor, c->channels[i], arg1);
        if (strcmp(arg1, "who") == 0)
        {
            raw_notify(executor, "");
        }
    }
}

void sort_users(struct channel *ch)
{
    int i;
    bool done = false;
    struct comuser *user;
    int nu = ch->num_users;

    while (!done)
    {
        done = true;
        for (i = 0; i < (nu - 1); i++)
        {
            if (ch->users[i]->who > ch->users[i + 1]->who)
            {
                user = ch->users[i];
                ch->users[i] = ch->users[i + 1];
                ch->users[i + 1] = user;
                done = false;
            }
        }
    }
}

void do_channelwho(dbref executor, dbref caller, dbref enactor, int eval, int key, char *arg1)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(eval);
    UNUSED_PARAMETER(key);

    if (!mudconf.have_comsys)
    {
        raw_notify(executor, "Comsys disabled.");
        return;
    }

    char channel[MAX_CHANNEL_LEN+1];
    char *s = arg1;
    char *t = channel;
    while (*s && *s != '/' && ((t - channel) < MAX_CHANNEL_LEN))
    {
        *t++ = *s++;
    }
    *t = 0;

    bool flag = false;
    if (*s && *(s + 1))
    {
        flag = (*(s + 1) == 'a');
    }

    struct channel *ch = select_channel(channel);
    if (!ch)
    {
        raw_notify(executor, tprintf("Unknown channel %s.", channel));
        return;
    }
    if ( !(  Comm_All(executor)
          || Controls(executor,ch->charge_who)))
    {
        raw_notify(executor, NOPERM_MESSAGE);
        return;
    }
    raw_notify(executor, tprintf("-- %s --", ch->name));
    raw_notify(executor, tprintf("%-29.29s %-6.6s %-6.6s", "Name", "Status", "Player"));
    struct comuser *user;
    char *buff;
    char temp[LBUF_SIZE];
    int i;
    for (i = 0; i < ch->num_users; i++)
    {
        user = ch->users[i];
        if (  (  flag
              || UNDEAD(user->who))
           && (  !Hidden(user->who)
              || Wizard_Who(executor)
              || See_Hidden(executor)))
        {
            buff = unparse_object(executor, user->who, false);
            mux_sprintf(temp, sizeof(temp), "%-29.29s %-6.6s %-6.6s", strip_ansi(buff),
                user->bUserIsOn ? "on " : "off",
                isPlayer(user->who) ? "yes" : "no ");
            raw_notify(executor, temp);
            free_lbuf(buff);
        }
    }
    raw_notify(executor, tprintf("-- %s --", ch->name));
}

static void do_comdisconnectraw_notify(dbref player, char *chan)
{
    struct channel *ch = select_channel(chan);
    if (!ch) return;

    struct comuser *cu = select_user(ch, player);
    if (!cu) return;

    if (  (ch->type & CHANNEL_LOUD)
       && cu->bUserIsOn
       && !Hidden(player))
    {
        char *messNormal, *messNoComtitle;
        BuildChannelMessage((ch->type & CHANNEL_SPOOF) != 0, ch->header, cu,
            (char *)":has disconnected.", &messNormal, &messNoComtitle);
        SendChannelMessage(player, ch, messNormal, messNoComtitle);
    }
}

static void do_comconnectraw_notify(dbref player, char *chan)
{
    struct channel *ch = select_channel(chan);
    if (!ch) return;
    struct comuser *cu = select_user(ch, player);
    if (!cu) return;

    if (  (ch->type & CHANNEL_LOUD)
       && cu->bUserIsOn
       && !Hidden(player))
    {
        char *messNormal, *messNoComtitle;
        BuildChannelMessage((ch->type & CHANNEL_SPOOF) != 0, ch->header, cu,
            (char *)":has connected.", &messNormal, &messNoComtitle);
        SendChannelMessage(player, ch, messNormal, messNoComtitle);
    }
}

static void do_comconnectchannel(dbref player, char *channel, char *alias, int i)
{
    struct comuser *user;

    struct channel *ch = select_channel(channel);
    if (ch)
    {
        for (user = ch->on_users;
        user && user->who != player;
        user = user->on_next) ;

        if (!user)
        {
            user = select_user(ch, player);
            if (user)
            {
                user->on_next = ch->on_users;
                ch->on_users = user;
            }
            else
            {
                raw_notify(player, tprintf("Bad Comsys Alias: %s for Channel: %s", alias + i * ALIAS_SIZE, channel));
            }
        }
    }
    else
    {
        raw_notify(player, tprintf("Bad Comsys Alias: %s for Channel: %s", alias + i * ALIAS_SIZE, channel));
    }
}

void do_comdisconnect(dbref player)
{
    comsys_t *c = get_comsys(player);
    int i;

    for (i = 0; i < c->numchannels; i++)
    {
        char *CurrentChannel = c->channels[i];
        bool bFound = false;
        int j;

        for (j = 0; j < i; j++)
        {
            if (strcmp(c->channels[j], CurrentChannel) == 0)
            {
                bFound = true;
                break;
            }
        }

        if (!bFound)
        {
            do_comdisconnectchannel(player, CurrentChannel);
            do_comdisconnectraw_notify(player, CurrentChannel);
        }
    }
}

void do_comconnect(dbref player)
{
    comsys_t *c = get_comsys(player);
    int i;

    for (i = 0; i < c->numchannels; i++)
    {
        char *CurrentChannel = c->channels[i];
        bool bFound = false;
        int j;

        for (j = 0; j < i; j++)
        {
            if (strcmp(c->channels[j], CurrentChannel) == 0)
            {
                bFound = true;
                break;
            }
        }

        if (!bFound)
        {
            do_comconnectchannel(player, CurrentChannel, c->alias, i);
            do_comconnectraw_notify(player, CurrentChannel);
        }
    }
}


void do_comdisconnectchannel(dbref player, char *channel)
{
    struct channel *ch = select_channel(channel);
    if (!ch)
    {
        return;
    }

    struct comuser *prevuser = NULL;
    struct comuser *user;
    for (user = ch->on_users; user;)
    {
        if (user->who == player)
        {
            if (prevuser)
            {
                prevuser->on_next = user->on_next;
            }
            else
            {
                ch->on_users = user->on_next;
            }
            return;
        }
        else
        {
            prevuser = user;
            user = user->on_next;
        }
    }
}

void do_editchannel
(
    dbref executor,
    dbref caller,
    dbref enactor,
    int   flag,
    int   nargs,
    char *arg1,
    char *arg2
)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(nargs);

    if (!mudconf.have_comsys)
    {
        raw_notify(executor, "Comsys disabled.");
        return;
    }
    struct channel *ch = select_channel(arg1);
    if (!ch)
    {
        raw_notify(executor, tprintf("Unknown channel %s.", arg1));
        return;
    }
    if ( !(  Comm_All(executor)
          || Controls(executor, ch->charge_who)))
    {
        raw_notify(executor, NOPERM_MESSAGE);
        return;
    }

    bool add_remove = true;
    char *s = arg2;
    if (*s == '!')
    {
        add_remove = false;
        s++;
    }
    switch (flag)
    {
    case 0:
        {
            dbref who = lookup_player(executor, arg2, true);
            if (Good_obj(who))
            {
                ch->charge_who = Owner(who);
                raw_notify(executor, "Set.");
            }
            else
            {
                raw_notify(executor, "Invalid player.");
            }
        }
        break;

    case 1:
        {
            int c_charge = mux_atol(arg2);
            if (  0 <= c_charge
               && c_charge <= MAX_COST)
            {
                ch->charge = c_charge;
                raw_notify(executor, "Set.");
            }
            else
            {
                raw_notify(executor, "That is not a reasonable cost.");
            }
        }
        break;

    case 3:
        {
            int access = 0;
            if (strcmp(s, "join") == 0)
            {
                access = CHANNEL_PLAYER_JOIN;
            }
            else if (strcmp(s, "receive") == 0)
            {
                access = CHANNEL_PLAYER_RECEIVE;
            }
            else if (strcmp(s, "transmit") == 0)
            {
                access = CHANNEL_PLAYER_TRANSMIT;
            }
            else
            {
                raw_notify(executor, "@cpflags: Unknown Flag.");
            }

            if (access)
            {
                if (add_remove)
                {
                    ch->type |= access;
                    raw_notify(executor, "@cpflags: Set.");
                }
                else
                {
                    ch->type &= ~access;
                    raw_notify(executor, "@cpflags: Cleared.");
                }
            }
        }
        break;

    case 4:
        {
            int access = 0;
            if (strcmp(s, "join") == 0)
            {
                access = CHANNEL_OBJECT_JOIN;
            }
            else if (strcmp(s, "receive") == 0)
            {
                access = CHANNEL_OBJECT_RECEIVE;
            }
            else if (strcmp(s, "transmit") == 0)
            {
                access = CHANNEL_OBJECT_TRANSMIT;
            }
            else
            {
                raw_notify(executor, "@coflags: Unknown Flag.");
            }

            if (access)
            {
                if (add_remove)
                {
                    ch->type |= access;
                    raw_notify(executor, "@coflags: Set.");
                }
                else
                {
                    ch->type &= ~access;
                    raw_notify(executor, "@coflags: Cleared.");
                }
            }
        }
        break;
    }
}

bool test_join_access(dbref player, struct channel *chan)
{
    if (Comm_All(player))
    {
        return true;
    }

    int access;
    if (isPlayer(player))
    {
        access = CHANNEL_PLAYER_JOIN;
    }
    else
    {
        access = CHANNEL_OBJECT_JOIN;
    }
    return (  (chan->type & access) != 0
           || could_doit(player, chan->chan_obj, A_LOCK));
}

bool test_transmit_access(dbref player, struct channel *chan)
{
    if (Comm_All(player))
    {
        return true;
    }

    int access;
    if (isPlayer(player))
    {
        access = CHANNEL_PLAYER_TRANSMIT;
    }
    else
    {
        access = CHANNEL_OBJECT_TRANSMIT;
    }
    return (  (chan->type & access) != 0
           || could_doit(player, chan->chan_obj, A_LUSE));

}

bool test_receive_access(dbref player, struct channel *chan)
{
    if (Comm_All(player))
    {
        return true;
    }

    int access;
    if (isPlayer(player))
    {
        access = CHANNEL_PLAYER_RECEIVE;
    }
    else
    {
        access = CHANNEL_OBJECT_RECEIVE;
    }
    return (  (chan->type & access) != 0
           || could_doit(player, chan->chan_obj, A_LENTER));

}

// true means continue, false means stop
//
bool do_comsystem(dbref who, char *cmd)
{
    char *t = strchr(cmd, ' ');
    if (  !t
       || t - cmd > MAX_ALIAS_LEN
       || t[1] == '\0')
    {
        // doesn't fit the pattern of "alias message"
        return true;
    }

    char alias[ALIAS_SIZE];
    memcpy(alias, cmd, t - cmd);
    alias[t - cmd] = '\0';

    char *ch = get_channel_from_alias(who, alias);
    if (ch[0] == '\0')
    {
        // not really an alias after all
        return true;
    }

    t++;
    do_processcom(who, ch, t);
    return false;
}

void do_cemit
(
    dbref executor,
    dbref caller,
    dbref enactor,
    int   key,
    int   nargs,
    char *chan,
    char *text
)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(nargs);

    if (!mudconf.have_comsys)
    {
        raw_notify(executor, "Comsys disabled.");
        return;
    }
    struct channel *ch = select_channel(chan);
    if (!ch)
    {
        raw_notify(executor, tprintf("Channel %s does not exist.", chan));
        return;
    }
    if (  !Controls(executor, ch->charge_who)
       && !Comm_All(executor))
    {
        raw_notify(executor, NOPERM_MESSAGE);
        return;
    }
    char *text2 = alloc_lbuf("do_cemit");
    if (key == CEMIT_NOHEADER)
    {
        mux_strncpy(text2, text, LBUF_SIZE-1);
    }
    else
    {
        mux_strncpy(text2, tprintf("%s %s", ch->header, text), LBUF_SIZE-1);
    }
    SendChannelMessage(executor, ch, text2, text2);
}

void do_chopen
(
    dbref executor,
    dbref caller,
    dbref enactor,
    int   key,
    int   nargs,
    char *chan,
    char *value
)
{
    UNUSED_PARAMETER(nargs);

    if (!mudconf.have_comsys)
    {
        raw_notify(executor, "Comsys disabled.");
        return;
    }
    if (key == CSET_LIST)
    {
        do_chanlist(executor, caller, enactor, 0, 1, NULL);
        return;
    }

    const char *msg = NULL;
    struct channel *ch = select_channel(chan);
    if (!ch)
    {
        msg = tprintf("@cset: Channel %s does not exist.", chan);
        raw_notify(executor, msg);
        return;
    }
    if (  !Controls(executor, ch->charge_who)
       && !Comm_All(executor))
    {
        raw_notify(executor, NOPERM_MESSAGE);
        return;
    }
    char *buff;
    dbref thing;

    switch (key)
    {
    case CSET_PUBLIC:
        ch->type |= CHANNEL_PUBLIC;
        msg = tprintf("@cset: Channel %s placed on the public listings.", chan);
        break;

    case CSET_PRIVATE:
        ch->type &= ~CHANNEL_PUBLIC;
        msg = tprintf("@cset: Channel %s taken off the public listings." ,chan);
        break;

    case CSET_LOUD:
        ch->type |= CHANNEL_LOUD;
        msg = tprintf("@cset: Channel %s now sends connect/disconnect msgs.", chan);
        break;

    case CSET_QUIET:
        ch->type &= ~CHANNEL_LOUD;
        msg = tprintf("@cset: Channel %s connect/disconnect msgs muted.", chan);
        break;

    case CSET_SPOOF:
        ch->type |= CHANNEL_SPOOF;
        msg = tprintf("@cset: Channel %s set spoofable.", chan);
        break;

    case CSET_NOSPOOF:
        ch->type &= ~CHANNEL_SPOOF;
        msg = tprintf("@cset: Channel %s set unspoofable.", chan);
        break;

    case CSET_OBJECT:
        init_match(executor, value, NOTYPE);
        match_everything(0);
        thing = match_result();

        if (thing == NOTHING)
        {
            ch->chan_obj = thing;
            msg = tprintf("Channel %s is now disassociated from any channel object.", ch->name);
        }
        else if (Good_obj(thing))
        {
            ch->chan_obj = thing;
            buff = unparse_object(executor, thing, false);
            msg = tprintf("Channel %s is now using %s as channel object.", ch->name, buff);
            free_lbuf(buff);
        }
        else
        {
            msg = tprintf("%d is not a valid channel object.", thing);
        }
        break;

    case CSET_HEADER:
        do_cheader(executor, chan, value);
        msg = "Set.";
        break;

    case CSET_LOG:
        if (do_chanlog(executor, chan, value))
        {
            msg = tprintf("@cset: Channel %s maximum history set.", chan);
        }
        else
        {
            msg = tprintf("@cset: Maximum history must be a number less than or equal to %d.", MAX_RECALL_REQUEST);
        }
        break;
    }
    raw_notify(executor, msg);
}

void do_chboot
(
    dbref executor,
    dbref caller,
    dbref enactor,
    int   key,
    int   nargs,
    char *channel,
    char *victim
)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(nargs);

    // I sure hope it's not going to be that long.
    //
    if (!mudconf.have_comsys)
    {
        raw_notify(executor, "Comsys disabled.");
        return;
    }
    struct channel *ch = select_channel(channel);
    if (!ch)
    {
        raw_notify(executor, "@cboot: Unknown channel.");
        return;
    }
    struct comuser *user = select_user(ch, executor);
    if (!user)
    {
        raw_notify(executor, "@cboot: You are not on that channel.");
        return;
    }
    if (  !Controls(executor, ch->charge_who)
       && !Comm_All(executor))
    {
        raw_notify(executor, "@cboot: You can't do that!");
        return;
    }
    dbref thing = match_thing(executor, victim);

    if (!Good_obj(thing))
    {
        return;
    }
    struct comuser *vu = select_user(ch, thing);
    if (!vu)
    {
        raw_notify(executor, tprintf("@cboot: %s is not on the channel.",
            Moniker(thing)));
        return;
    }

    raw_notify(executor, tprintf("You boot %s off channel %s.",
                                 Moniker(thing), ch->name));
    raw_notify(thing, tprintf("%s boots you off channel %s.",
                              Moniker(thing), ch->name));

    if (!(key & CBOOT_QUIET))
    {
        char *mess1, *mess1nct;
        char *mess2, *mess2nct;
        BuildChannelMessage((ch->type & CHANNEL_SPOOF) != 0, ch->header, user,
                            (char *)":boots", &mess1, &mess1nct);
        BuildChannelMessage((ch->type & CHANNEL_SPOOF) != 0, 0, vu,
                            (char *)":off the channel.", &mess2, &mess2nct);
        char *messNormal = alloc_lbuf("do_chboot.messnormal");
        char *messNoComtitle = alloc_lbuf("do_chboot.messnocomtitle");
        char *mnp = messNormal;
        char *mnctp = messNoComtitle;
        if (mess1)
        {
            safe_str(mess1, messNormal, &mnp);
            free_lbuf(mess1);
        }
        if (mess2)
        {
            safe_str(mess2, messNormal, &mnp);
            free_lbuf(mess2);
        }
        *mnp = '\0';
        if (mess1nct)
        {
            safe_str(mess1nct, messNoComtitle, &mnctp);
            free_lbuf(mess1nct);
        }
        if (mess2nct)
        {
            safe_str(mess2nct, messNoComtitle, &mnctp);
            free_lbuf(mess2nct);
        }
        *mnctp = '\0';
        SendChannelMessage(executor, ch, messNormal, messNoComtitle);
        do_delcomchannel(thing, channel, false);
    }
    else
    {
        do_delcomchannel(thing, channel, true);
    }
}

void do_cheader(dbref player, char *channel, char *header)
{
    struct channel *ch = select_channel(channel);
    if (!ch)
    {
        raw_notify(player, "That channel does not exist.");
        return;
    }
    if (  !Controls(player, ch->charge_who)
       && !Comm_All(player))
    {
        raw_notify(player, NOPERM_MESSAGE);
        return;
    }
    char *p = RemoveSetOfCharacters(header, "\r\n\t");

    // Optimize/terminate any ANSI in the string.
    //
    char NewHeader_ANSI[MAX_HEADER_LEN+1];
    size_t nVisualWidth;
    size_t nLen = ANSI_TruncateToField(p, sizeof(NewHeader_ANSI),
        NewHeader_ANSI, sizeof(NewHeader_ANSI), &nVisualWidth,
        ANSI_ENDGOAL_NORMAL);
    memcpy(ch->header, NewHeader_ANSI, nLen+1);
}

struct chanlist_node
{
    char *           name;
    struct channel * ptr;
};

static int DCL_CDECL chanlist_comp(const void* a, const void* b)
{
    chanlist_node* ca = (chanlist_node*)a;
    chanlist_node* cb = (chanlist_node*)b;
    return mux_stricmp(ca->name, cb->name);
}

void do_chanlist
(
    dbref executor,
    dbref caller,
    dbref enactor,
    int   eval,
    int   key,
    char *pattern
)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(eval);

    if (!mudconf.have_comsys)
    {
        raw_notify(executor, "Comsys disabled.");
        return;
    }
    if (key & CLIST_FULL)
    {
        do_listchannels(executor);
        return;
    }

    dbref owner;
    struct channel *ch;
    int flags = 0;
    char *atrstr;
    char *temp = alloc_mbuf("do_chanlist_temp");
    char *buf = alloc_mbuf("do_chanlist_buf");

    if (key & CLIST_HEADERS)
    {
        raw_notify(executor, "*** Channel       Owner           Header");
    }
    else
    {
        raw_notify(executor, "*** Channel       Owner           Description");
    }

    bool bWild;
    if (  NULL != pattern
       && '\0' != *pattern)
    {
        bWild = true;
    }
    else
    {
        bWild = false;
    }

#define MAX_SUPPORTED_NUM_ENTRIES 10000

    unsigned int entries = mudstate.channel_htab.GetEntryCount();
    if (MAX_SUPPORTED_NUM_ENTRIES < entries)
    {
        // Nobody should have so many channels.
        //
        entries = MAX_SUPPORTED_NUM_ENTRIES;
    }

    if (0 < entries)
    {
        struct chanlist_node* charray =
            (chanlist_node*)MEMALLOC(sizeof(chanlist_node)*entries);

        if (charray)
        {
            // Arrayify all the channels
            //
            size_t actualEntries = 0;
            for (  ch = (struct channel *)hash_firstentry(&mudstate.channel_htab);
                   ch
                && actualEntries < entries;
                   ch = (struct channel *)hash_nextentry(&mudstate.channel_htab))
            {
                if (  !bWild
                   || quick_wild(pattern, ch->name))
                {
                    charray[actualEntries].name = ch->name;
                    charray[actualEntries].ptr = ch;
                    actualEntries++;
                }
            }

            if (0 < actualEntries)
            {
                qsort(charray, actualEntries, sizeof(struct chanlist_node), chanlist_comp);

                for (size_t i = 0; i < actualEntries; i++)
                {
                    ch = charray[i].ptr;
                    if (  Comm_All(executor)
                       || (ch->type & CHANNEL_PUBLIC)
                       || Controls(executor, ch->charge_who))
                    {
                        char *pBuffer;
                        if (key & CLIST_HEADERS)
                        {
                            pBuffer = ch->header;
                        }
                        else
                        {
                            atrstr = atr_pget(ch->chan_obj, A_DESC, &owner, &flags);
                            if (  NOTHING == ch->chan_obj
                               || !*atrstr)
                            {
                                mux_strncpy(buf, "No description.", MBUF_SIZE-1);
                            }
                            else
                            {
                                mux_sprintf(buf, MBUF_SIZE, "%-54.54s", atrstr);
                            }
                            free_lbuf(atrstr);

                            pBuffer = buf;
                        }

                        char *ownername_ansi = ANSI_TruncateAndPad_sbuf(Moniker(ch->charge_who), 15);
                        mux_sprintf(temp, MBUF_SIZE, "%c%c%c %-13.13s %s %-45.45s",
                            (ch->type & (CHANNEL_PUBLIC)) ? 'P' : '-',
                            (ch->type & (CHANNEL_LOUD)) ? 'L' : '-',
                            (ch->type & (CHANNEL_SPOOF)) ? 'S' : '-',
                            ch->name, ownername_ansi, pBuffer);
                        free_sbuf(ownername_ansi);

                        raw_notify(executor, temp);
                    }
                }
            }
            MEMFREE(charray);
        }
    }
    free_mbuf(temp);
    free_mbuf(buf);
    raw_notify(executor, "-- End of list of Channels --");
}

// Returns a player's comtitle for a named channel.
//
FUNCTION(fun_comtitle)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(eval);
    UNUSED_PARAMETER(nfargs);
    UNUSED_PARAMETER(cargs);
    UNUSED_PARAMETER(ncargs);

    if (!mudconf.have_comsys)
    {
        safe_str("#-1 COMSYS DISABLED", buff, bufc);
        return;
    }

    dbref victim = lookup_player(executor, fargs[0], true);
    if (!Good_obj(victim))
    {
        init_match(executor, fargs[0], TYPE_THING);
        match_everything(0);
        victim = match_result();
        if (!Good_obj(victim))
        {
            safe_str("#-1 OBJECT DOES NOT EXIST", buff, bufc);
            return;
        }
    }

    struct channel *chn = select_channel(fargs[1]);
    if (!chn)
    {
        safe_str("#-1 CHANNEL DOES NOT EXIST", buff, bufc);
        return;
    }

    comsys_t *c = get_comsys(executor);
    struct comuser *user;

    int i;
    bool onchannel = false;
    if (Wizard(executor))
    {
        onchannel = true;
    }
    else
    {
        for (i = 0; i < c->numchannels; i++)
        {
            user = select_user(chn, executor);
            if (user)
            {
                onchannel = true;
                break;
            }
        }
    }

    if (!onchannel)
    {
        safe_noperm(buff, bufc);
        return;
    }

    for (i = 0; i < c->numchannels; i++)
    {
        user = select_user(chn, victim);
        if (user)
        {
          // Do we want this function to evaluate the comtitle or not?
#if 0
          char *nComTitle = GetComtitle(user);
          safe_str(nComTitle, buff, bufc);
          FreeComtitle(nComTitle);
          return;
#else
          safe_str(user->title, buff, bufc);
          return;
#endif
        }
    }
    safe_str("#-1 OBJECT NOT ON THAT CHANNEL", buff, bufc);
}

// Returns a player's comsys alias for a named channel.
//
FUNCTION(fun_comalias)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(eval);
    UNUSED_PARAMETER(nfargs);
    UNUSED_PARAMETER(cargs);
    UNUSED_PARAMETER(ncargs);

    if (!mudconf.have_comsys)
    {
        safe_str("#-1 COMSYS DISABLED", buff, bufc);
        return;
    }

    dbref victim = lookup_player(executor, fargs[0], true);
    if (!Good_obj(victim))
    {
        init_match(executor, fargs[0], TYPE_THING);
        match_everything(0);
        victim = match_result();
        if (!Good_obj(victim))
        {
            safe_str("#-1 OBJECT DOES NOT EXIST", buff, bufc);
            return;
        }
    }

    struct channel *chn = select_channel(fargs[1]);
    if (!chn)
    {
        safe_str("#-1 CHANNEL DOES NOT EXIST", buff, bufc);
        return;
    }

    // Wizards can get the comalias for anyone. Players and objects can check
    // for themselves. Objects that Inherit can check for their owners.
    //
    if (  !Wizard(executor)
       && executor != victim
       && (  Owner(executor) != victim
          || !Inherits(executor)))
    {
        safe_noperm(buff, bufc);
        return;
    }

    comsys_t *cc = get_comsys(victim);
    for (int i = 0; i < cc->numchannels; i++)
    {
        if (!strcmp(fargs[1], cc->channels[i]))
        {
            safe_str(cc->alias + i * ALIAS_SIZE, buff, bufc);
            return;
        }
    }
    safe_str("#-1 OBJECT NOT ON THAT CHANNEL", buff, bufc);
}

// Returns a list of channels.
//
FUNCTION(fun_channels)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(eval);
    UNUSED_PARAMETER(cargs);
    UNUSED_PARAMETER(ncargs);

    if (!mudconf.have_comsys)
    {
        safe_str("#-1 COMSYS DISABLED", buff, bufc);
        return;
    }

    dbref who = NOTHING;
    if (nfargs >= 1)
    {
        who = lookup_player(executor, fargs[0], true);
        if (  who == NOTHING
           && mux_stricmp(fargs[0], "all") != 0)
        {
            safe_str("#-1 PLAYER NOT FOUND", buff, bufc);
            return;
        }
    }

    ITL itl;
    ItemToList_Init(&itl, buff, bufc);
    struct channel *chn;
    for (chn = (struct channel *)hash_firstentry(&mudstate.channel_htab);
         chn;
         chn = (struct channel *)hash_nextentry(&mudstate.channel_htab))
    {
        if (  (  Comm_All(executor)
              || (chn->type & CHANNEL_PUBLIC)
              || Controls(executor, chn->charge_who))
           && (  who == NOTHING
              || Controls(who, chn->charge_who))
           && !ItemToList_AddString(&itl, chn->name))
        {
            break;
        }
    }
}
