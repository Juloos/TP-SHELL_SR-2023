/*
 * Copyright (C) 2002, Simon Nieuviarts
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include "readcmd.h"


static void memory_error(void) {
    errno = ENOMEM;
    perror(0);
    exit(1);
}


static void *xmalloc(size_t size) {
    void *p = malloc(size);
    if (!p) memory_error();
    return p;
}


static void *xrealloc(void *ptr, size_t size) {
    void *p = realloc(ptr, size);
    if (!p) memory_error();
    return p;
}


/* Read a line from standard input and put it in a char[] */
static char *readline(void) {
    size_t buf_len = 16;
    char *buf = xmalloc(buf_len * sizeof(char));

    if (fgets(buf, buf_len, stdin) == NULL) {
        free(buf);
        return NULL;
    }

    if (feof(stdin)) { /* End of file (ctrl-d) */
        fflush(stdout);
        exit(0);
    }

    do {
        size_t l = strlen(buf);
        if ((l > 0) && (buf[l - 1] == '\n')) {
            l--;
            buf[l] = 0;
            return buf;
        }
        if (buf_len >= (INT_MAX / 2)) memory_error();
        buf_len *= 2;
        buf = xrealloc(buf, buf_len * sizeof(char));
        if (fgets(buf + l, buf_len - l, stdin) == NULL) return buf;
    } while (1);
}


/* Split the string in words, according to the simple shell grammar. */
static char **split_in_words(char *line) {
    char *cur = line;
    char **tab = 0;
    size_t l = 0;
    char c;
    int escape = 0;

    while ((c = *cur) != 0) {
        char *w = 0;
        char *start;
        switch (c) {
            case ' ':
            case '\t':
                /* Ignore any whitespace */
                cur++;
                break;
            case '<':
                w = "<";
                cur++;
                break;
            case '>':
                w = ">";
                cur++;
                break;
            case '|':
                w = "|";
                cur++;
                break;
            case '&':
                w = "&";
                cur++;
                break;
            case '\'':
                escape = 1;
            case '"':
                start = ++cur;
                // While current char is not null, and not end of quote
                int escaped_c = 0;
                while ((c = *cur) && (c != start[-1] || escaped_c)) {
                    cur++;
                    // if current char is backslash and escape is disabled and current char is not escaped
                    if (c == '\\' && !escape && !escaped_c)
                        escaped_c = 1;
                    else
                        escaped_c = 0;
                }
                if (c == 0) {
                    errno = EINVAL;
                    perror(0);
                }
                w = xmalloc((cur - start + 1) * sizeof(char));
                strncpy(w, start, cur - start);
                w[cur - start] = 0;
                cur++;
                escape = 0;
                break;
            default:
                /* Another word */
                start = cur;
                while (c) {
                    c = *cur++;
                    switch (c) {
                        case 0:
                        case ' ':
                        case '\t':
                        case '<':
                        case '>':
                        case '|':
                        case '&':
                            c = 0;
                            break;
                        case '\\':
                            cur++;
                        default:;
                    }
                }
                w = xmalloc((cur - start) * sizeof(char));
                strncpy(w, start, cur - start - 1);
                w[cur - start - 1] = 0;
        }
        if (w) {
            tab = xrealloc(tab, (l + 1) * sizeof(char *));
            tab[l++] = w;
        }
    }
    tab = xrealloc(tab, (l + 1) * sizeof(char *));
    tab[l++] = 0;
    return tab;
}


static void freeseq(char ***seq) {
    int i, j;

    for (i = 0; seq[i] != 0; i++) {
        char **cmd = seq[i];

        for (j = 0; cmd[j] != 0; j++) free(cmd[j]);
        free(cmd);
    }
    free(seq);
}


/* Free the fields of the structure but not the structure itself */
static void freecmd(struct cmdline *s) {
    if (s->in) free(s->in);
    if (s->out) free(s->out);
    if (s->seq) freeseq(s->seq);
}

void freecmd2(struct cmdline *s) {
    if (s->err) free(s->err);
    freecmd(s);
    free(s);
}


struct cmdline *readcmd(void) {
    static struct cmdline *static_cmdline = 0;
    struct cmdline *s = static_cmdline;
    char *line;
    char **words;
    int i;
    char *w;
    char **cmd;
    char ***seq;
    size_t cmd_len, seq_len;

    line = readline();
    if (line == NULL) {
        if (s) {
            freecmd(s);
            free(s);
        }
        return static_cmdline = 0;
    }

    cmd = xmalloc(sizeof(char *));
    cmd[0] = 0;
    cmd_len = 0;
    seq = xmalloc(sizeof(char **));
    seq[0] = 0;
    seq_len = 0;

    words = split_in_words(line);
    free(line);

    if (!s)
        static_cmdline = s = xmalloc(sizeof(struct cmdline));
    else
        freecmd(s);
    s->bg = 0;
    s->err = 0;
    s->in = 0;
    s->out = 0;
    s->seq = 0;

    int k;
    size_t len;

    i = 0;
    while ((w = words[i++]) != 0) {
        switch (w[0]) {
            case '<':
                /* Tricky : the word can only be "<" */
                if (s->in) {
                    s->err = "only one input file supported";
                    goto error;
                }
                if (words[i] == 0) {
                    s->err = "filename missing for input redirection";
                    goto error;
                }
                s->in = words[i++];
                break;
            case '>':
                /* Tricky : the word can only be ">" */
                if (s->out) {
                    s->err = "only one output file supported";
                    goto error;
                }
                if (words[i] == 0) {
                    s->err = "filename missing for output redirection";
                    goto error;
                }
                s->out = words[i++];
                break;
            case '|':
                /* Tricky : the word can only be "|" */
                if (cmd_len == 0) {
                    s->err = "misplaced pipe";
                    goto error;
                }

                seq = xrealloc(seq, (seq_len + 2) * sizeof(char **));
                seq[seq_len++] = cmd;
                seq[seq_len] = 0;

                cmd = xmalloc(sizeof(char *));
                cmd[0] = 0;
                cmd_len = 0;
                break;
            case '&':
                /* Tricky : the word can only be "&" */
                // Background operator should be the last word, otherwise it was misplaced
                if (words[i] != 0 || w[1] != 0) {
                    // If the next character is '&' then the user tried to use '&&', which is not supported
                    if (strcmp(words[i], "&") == 0)
                        s->err = "only one background operator supported";
                    else
                        s->err = "misplaced background operator";
                    goto error;
                }
                // There should be only one background operator
                if (s->bg) {
                    s->err = "only one background operator supported";
                    goto error;
                }
                s->bg = 1;
                break;
            default:
                // Change the word to delete the escaping backslashes
                k = 0;
                len = strlen(w);
                for (int j = 0; j < len; j++) {
                    if (w[k + j] == '\\')
                        w[j] = w[++k + j];
                    else
                        w[j] = w[k + j];
                }
                w[len - k] = 0;
                cmd = xrealloc(cmd, (cmd_len + 2) * sizeof(char *));
                cmd[cmd_len++] = w;
                cmd[cmd_len] = 0;
        }
    }

    if (cmd_len != 0) {
        seq = xrealloc(seq, (seq_len + 2) * sizeof(char **));
        seq[seq_len++] = cmd;
        seq[seq_len] = 0;
    } else if (seq_len != 0) {
        s->err = "misplaced pipe";
        i--;
        goto error;
    } else
        free(cmd);
    free(words);
    s->seq = seq;
    return s;
    error:
    while ((w = words[i++]) != 0) {
        switch (w[0]) {
            case '<':
            case '>':
            case '|':
            case '&':
                break;
            default:
                free(w);
        }
    }
    free(words);
    freeseq(seq);
    for (i = 0; cmd[i] != 0; i++) free(cmd[i]);
    free(cmd);
    if (s->in) {
        free(s->in);
        s->in = 0;
    }
    if (s->out) {
        free(s->out);
        s->out = 0;
    }
    return s;
}
