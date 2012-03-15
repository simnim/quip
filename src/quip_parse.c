/*
 * This file is part of quip.
 *
 * Copyright (c) 2011 by Daniel C. Jones <dcjones@cs.washington.edu>
 *
 */

#include "quip_parse.h"
#include "misc.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>


static const char   qual_first = 33;

static const size_t init_str_size  = 128;
static const size_t fastq_buf_size = 1048576;

static void fastq_alloc_str(str_t* s)
{
    s->s = malloc_or_die(init_str_size);
    s->s[0] = '\0';
    s->n = 0;
    s->size = init_str_size;
}


void fastq_expand_str(str_t* s)
{
    s->size *= 2;
    s->s = realloc_or_die(s->s, s->size);
}


seq_t* fastq_alloc_seq()
{
    seq_t* seq = malloc_or_die(sizeof(seq_t));
    fastq_alloc_str(&seq->id1);
    fastq_alloc_str(&seq->seq);
    fastq_alloc_str(&seq->id2);
    fastq_alloc_str(&seq->qual);

    return seq;
}


void fastq_copy_seq(seq_t* dest, const seq_t* src)
{
    while (dest->id1.size  < src->id1.n + 1)  fastq_expand_str(&dest->id1);
    memcpy(dest->id1.s, src->id1.s, src->id1.n);
    dest->id1.n = src->id1.n;
    dest->id1.s[dest->id1.n] = '\0';

    while (dest->seq.size  < src->seq.n + 1)  fastq_expand_str(&dest->seq);
    memcpy(dest->seq.s, src->seq.s, src->seq.n);
    dest->seq.n = src->seq.n;
    dest->seq.s[dest->seq.n] = '\0';

    while (dest->id2.size  < src->id2.n + 1)  fastq_expand_str(&dest->id2);
    memcpy(dest->id2.s, src->id2.s, src->id2.n);
    dest->id2.n = src->id2.n;
    dest->id2.s[dest->id2.n] = '\0';

    while (dest->qual.size < src->qual.n + 1) fastq_expand_str(&dest->qual);
    memcpy(dest->qual.s, src->qual.s, src->qual.n);
    dest->qual.n = src->qual.n;
    dest->qual.s[dest->qual.n] = '\0';
}


void fastq_free_seq(seq_t* seq)
{
    free(seq->id1.s);
    free(seq->seq.s);
    free(seq->id2.s);
    free(seq->qual.s);
    free(seq);
}


typedef enum
{
    STATE_EOF,
    STATE_ERROR,
    STATE_ID1,
    STATE_SEQ,
    STATE_ID2,
    STATE_QUAL

} fastq_state;


fastq_t* fastq_open(FILE* f)
{
    fastq_t* fqf = malloc_or_die(sizeof(fastq_t));

    fqf->file   = f;
    fqf->state  = STATE_ID1;
    fqf->buf    = malloc_or_die(fastq_buf_size);
    fqf->buf[0] = '\0';
    fqf->c      = fqf->buf;

    return fqf;
}


void fastq_close(fastq_t* fqf)
{
    free(fqf->buf);
    free(fqf);
}


void fastq_refill(fastq_t* f)
{
    int errnum;

    size_t n = fread(f->buf, sizeof(char), fastq_buf_size - 1, f->file);

    if (n <= 0) {
        if (feof(f->file)) {
            f->state = STATE_EOF;
            n = 0;
        }
        else {
            errnum = ferror(f->file);
            fprintf(stderr, "I/O error: %d\n", errnum);
            exit(1);
        }
    }

    f->buf[n] = '\0';
    f->c = f->buf;
}


void fastq_get_line(fastq_t* f, str_t* s)
{
    s->n = 0;

    size_t i = 0;
    while (1) {
        while (f->c[i] >= '\r') ++i;

        if (f->c[i] == '\0') {
            if (s)  {
                while (s->size - s->n < i + 2) {
                    fastq_expand_str(s);
                }

                memcpy(s->s + s->n, f->c, i);
                s->n += i;
            }

            fastq_refill(f);
            i = 0;
            if (f->state == STATE_EOF) break;
        }
        else {
            if (s)  {
                while (s->size - s->n < i + 2) {
                    fastq_expand_str(s);
                }

                memcpy(s->s + s->n, f->c, i);
                s->n += i;
            }

            f->c += i;

            break;
        }
    }


    if (s) {
        if (s->n > 0 && s->s[s->n - 1] == '\r') s->n--;
        s->s[s->n] = '\0';
    }
}



int fastq_next(fastq_t* f, seq_t* seq)
{
    size_t i;

    if (f->state == STATE_EOF) return 0;

    while (1) {

        /* read more, if needed */
        if (*f->c == '\0' ) {
            fastq_refill(f);
            if (f->state == STATE_EOF) return 0;
            continue;
        }

        /* skip over leading whitespace */
        else if (isspace(*f->c)) {
            f->c++;
            continue;
        }

        switch (f->state) {
            case STATE_ID1:
                if (*f->c == '@' || *f->c == '>') {
                    f->c++;
                    fastq_get_line(f, &seq->id1);
                    if (f->state == STATE_EOF) return 0;

                    f->state = STATE_SEQ;
                }
                else {
                    fprintf(stderr,
                            "Malformed FASTQ file: expecting an '@' or '>', saw a '%c'\n",
                            *f->c);
                    exit(1);
                }
                break;


            case STATE_SEQ:
                fastq_get_line(f, &seq->seq);
                if (f->state == STATE_EOF) return 0;

                f->state = STATE_ID2;
                break;


            case STATE_ID2:
                if (*f->c == '+') {
                    f->c++;
                    fastq_get_line(f, &seq->id2);
                    if (f->state == STATE_EOF) return 0;

                    f->state = STATE_QUAL;
                }
                else {
                    /* fasta style entry */
                    seq->id2.s[0]  = '\0';
                    seq->qual.s[0] = '\0';

                    f->state = STATE_ID1;
                    return 1;
                }
                break;


            case STATE_QUAL:
                fastq_get_line(f, &seq->qual);

                /* subtract the first printable ascii character to get quality
                 * scores starting at 0 */
                for (i = 0; i < seq->qual.n; ++i) seq->qual.s[i] -= qual_first;

                if (f->state == STATE_EOF) return 1;

                f->state = STATE_ID1;
                return 1;
        }

        f->c++;
    }

    return 1;
}


void fastq_rewind(fastq_t* fqf)
{
    rewind(fqf->file);
    fqf->state = STATE_ID1;
    fqf->buf[0] = '\0';
    fqf->c = fqf->buf;
}
