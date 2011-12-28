
#include "kmerhash.h"
#include "misc.h"
#include <string.h>
#include <assert.h>

static const size_t kmerhash_initial_size = 1024;
static const size_t kmerhash_max_load     = 5.0;

typedef unsigned char* slot_t;

struct kmerhash_t_
{
    size_t n;     /* number of slots */
    size_t m;     /* number of pairs stored */
    size_t m_max; /* number of stored pairs before we expand */

    size_t* slot_sizes;
    slot_t* slots;
};


kmerhash_t* kmerhash_alloc()
{
    kmerhash_t* H = malloc_or_die(sizeof(kmerhash_t));
    H->n = kmerhash_initial_size;
    H->m = 0;
    H->m_max = (size_t) (kmerhash_max_load * (double) H->n);

    H->slot_sizes = malloc_or_die(H->n * sizeof(size_t));
    memset(H->slot_sizes, 0, H->n * sizeof(size_t));

    H->slots = malloc_or_die(H->n * sizeof(slot_t*));
    memset(H->slots, 0, H->n * sizeof(slot_t*));

    return H;
}


void kmerhash_free(kmerhash_t* H)
{
    size_t i;
    for (i = 0; i < H->n; ++i) {
        free(H->slots[i]);
    }
    free(H->slots);

    free(H->slot_sizes);
    free(H);
}


size_t kmerhash_size(kmerhash_t* H)
{
    return H->m;
}


static void kmerhash_expand(kmerhash_t* H)
{
    size_t new_n = 2 * H->n;
    size_t* slot_sizes = malloc_or_die(new_n * sizeof(size_t));
    memset(slot_sizes, 0, new_n * sizeof(size_t));


    /* Figure out the slot sizes for the new table. */
    kmer_t key;
    uint8_t len;
    size_t i;
    slot_t c0, c;
    for (i = 0; i < H->n; ++i) {
        c0 = H->slots[i];
        for (c = c0; (size_t) (c - c0) < H->slot_sizes[i]; ) {
            key = *(kmer_t*) c;
            c += sizeof(kmer_t);

            len = *(uint8_t*) c;
            c += sizeof(uint8_t);

            slot_sizes[kmer_hash(key) % new_n] +=
                sizeof(kmer_t) + sizeof(uint8_t) + len * sizeof(kmer_pos_t);

            c += len * sizeof(kmer_pos_t);
        }
    }
    

    /* Allocate slots. */
    slot_t* slots = malloc_or_die(new_n * sizeof(slot_t));
    for (i = 0; i < new_n; ++i) {
        if (slot_sizes[i] > 0) {
            slots[i] = malloc_or_die(slot_sizes[i]);
        }
        else slots[i] = NULL;
    }


    /* Rehash values. */
    size_t m = 0;
    uint64_t h;
    for (i = 0; i < H->n; ++i) {
        c0 = H->slots[i];
        for (c = c0; (size_t) (c - c0) < H->slot_sizes[i]; ) {
            key = *(kmer_t*) c;
            c += sizeof(kmer_t);

            len = *(uint8_t*) c;
            c += sizeof(uint8_t);

            h = kmer_hash(key) % new_n;

            *(kmer_t*) slots[h] = key;
            slots[h] += sizeof(kmer_t);

            *(uint8_t*) slots[h] = len;
            slots[h] += sizeof(uint8_t);

            memcpy(slots[h], c, len * sizeof(kmer_pos_t));
            slots[h] += len * sizeof(kmer_pos_t);
            c        += len * sizeof(kmer_pos_t);

            ++m;
        }

    }

    for (i = 0; i < new_n; ++i) {
        slots[i] -= slot_sizes[i];
    }

    assert(m == H->m);

    for (i = 0; i < H->n; ++i) free(H->slots[i]);
    free(H->slots);
    H->slots = slots;

    free(H->slot_sizes);
    H->slot_sizes = slot_sizes;

    H->n = new_n;
    H->m_max = (size_t) (kmerhash_max_load * (double) H->n);
}


void kmerhash_put(kmerhash_t* H, kmer_t x, uint32_t contig_idx, int32_t contig_pos)
{
    /* if we are at capacity, preemptively expand */
    if (H->m >= H->m_max) kmerhash_expand(H);

    size_t old_size;

    kmer_t y;
    slot_t c0, c;
    uint8_t len;
    uint64_t h = kmer_hash(x) % H->n;
    c0 = c = H->slots[h];
    while ((size_t) (c - c0) < H->slot_sizes[h]) {
        y = *(kmer_t*) c;
        if (x == y) {
            old_size = H->slot_sizes[h];
            H->slot_sizes[h] += sizeof(kmer_pos_t);
            H->slots[h] = realloc_or_die(H->slots[h], H->slot_sizes[h]);

            c  = H->slots[h] + (c - c0);
            c0 = H->slots[h];

            c += sizeof(kmer_t);

            /* shift everything over to make space */
            memmove(c + sizeof(uint8_t) + sizeof(kmer_pos_t),
                    c + sizeof(uint8_t),
                    (c0 + H->slot_sizes[h]) - (c + sizeof(uint8_t) + sizeof(kmer_pos_t)));

            *(uint8_t*) c += 1;
            c += sizeof(uint8_t);

            ((kmer_pos_t*) c)->contig_idx = contig_idx;
            ((kmer_pos_t*) c)->contig_pos = contig_pos;

            return;
        }
        else {
            c += sizeof(kmer_t);
            len = *(uint8_t*) c;
            c += sizeof(uint8_t);
            c += len * (sizeof(uint32_t) + sizeof(int32_t));
        }
    }

    /* x is not present in the table, insert it */
    old_size = H->slot_sizes[h];
    H->slot_sizes[h] +=
            sizeof(kmer_t) + sizeof(uint8_t) + sizeof(kmer_pos_t);
    H->slots[h] = realloc_or_die(H->slots[h], H->slot_sizes[h]);

    c = H->slots[h] + old_size;
    *(kmer_t*) c = x;
    c += sizeof(kmer_t);

    *(uint8_t*) c = 1;
    c += sizeof(uint8_t);

    ((kmer_pos_t*) c)->contig_idx = contig_idx;
    ((kmer_pos_t*) c)->contig_pos = contig_pos;

    H->m++;
}


size_t kmerhash_get(kmerhash_t* H, kmer_t x, kmer_pos_t** pos)
{
    /* if we are at capacity, preemptively expand */
    if (H->m >= H->m_max) kmerhash_expand(H);

    kmer_t y;
    slot_t c0, c;
    size_t len;
    uint64_t h = kmer_hash(x) % H->n;
    c0 = c = H->slots[h];
    while ((size_t) (c - c0) < H->slot_sizes[h]) {
        y = *(kmer_t*) c;
        c += sizeof(kmer_t);

        len = *(uint8_t*) c;
        c += sizeof(uint8_t);

        if (x == y) {
            *pos = (kmer_pos_t*) c;
            return len;
        }
        else {
            c += len * (sizeof(kmer_pos_t));
        }
    }

    return 0;
}
