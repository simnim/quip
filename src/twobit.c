
#include "twobit.h"
#include "kmer.h"
#include "misc.h"
#include <string.h>


/* the number of kmer_t elements needed to represent a sequence of the given
 * length */
static size_t kmers_needed(size_t len)
{
    if (len == 0) return 0;
    else          return (len - 1) / (4 * sizeof(kmer_t)) + 1;
}


struct twobit_t_
{
    size_t len; /* length of stored sequnce */
    size_t n;   /* space (number of kmers) allocated in seq */
    kmer_t* seq;
};


twobit_t* twobit_alloc_n(size_t len)
{
    twobit_t* s = malloc_or_die(sizeof(twobit_t));
    s->len = 0;
    s->n   = kmers_needed(len);
    s->seq = malloc_or_die(s->n * sizeof(kmer_t));

    return s;
}


twobit_t* twobit_alloc()
{
    return twobit_alloc_n(512);
}


void twobit_free(twobit_t* s)
{
    free(s->seq);
    free(s);
}


size_t twobit_len(twobit_t* s)
{
    return s->len;
}


void twobit_append(twobit_t* s, const char* seqstr)
{
    twobit_append_n(s, seqstr, strlen(seqstr));
}


void twobit_append_n(twobit_t* s, const char* seqstr, size_t seqlen)
{
    /* expand, if more space is needed */
    if (s->n < kmers_needed(s->len + seqlen)) {
        while (s->n < kmers_needed(s->len + seqlen)) s->n *= 2;
        s->seq = realloc_or_die(s->seq, s->n * sizeof(kmer_t));
    }

    kmer_t c;
    size_t idx, off;
    size_t i;
    for (i = 0; i < seqlen; ++i) {
        c = chartokmer(seqstr[i]);
        if (c > 3) continue;

        idx = (s->len + i) / (4 * sizeof(kmer_t));
        off = (s->len + i) % (4 * sizeof(kmer_t));

#ifdef WORDS_BIGENDIAN
        s->seq[idx] = (s->seq[idx] & ~((kmer_t) 0x3 >> (2 * off))) | (c >> (2 * off));
#else
        s->seq[idx] = (s->seq[idx] & ~((kmer_t) 0x3 << (2 * off))) | (c << (2 * off));
#endif
    }

    s->len += seqlen;
}



void twobit_set(size_t i, twobit_t* s, char seqc)
{
    size_t idx = i / (4 * sizeof(kmer_t));
    size_t off = i % (4 * sizeof(kmer_t));

    kmer_t c = chartokmer(seqc); 
    if (c > 3) return;

#ifdef WORDS_BIGENDIAN
    s->seq[idx] = (s->seq[idx] & ~((kmer_t) 0x3 >> (2 * off))) | (c >> (2 * off));
#else
    s->seq[idx] = (s->seq[idx] & ~((kmer_t) 0x3 << (2 * off))) | (c << (2 * off));
#endif

}

kmer_t twobit_get(size_t i, twobit_t* s)
{
    size_t idx = i / (4 * sizeof(kmer_t));
    size_t off = i % (4 * sizeof(kmer_t));

#ifdef WORDS_BIGENDIAN
    return (s->seq[idx] << (2 * off)) & 0x3;
#else
    return (s->seq[idx] >> (2 * off)) & 0x3;
#endif
}


