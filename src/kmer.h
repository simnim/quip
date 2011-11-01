
#ifndef FAZER_KMER
#define FAZER_KMER

#include <stdlib.h>
#include <stdint.h>

/* K-mers are encoded 2 bits per nucleotide in a 64 bit integer,
 * allowing up to k = 32.
 */
typedef uint64_t kmer_t;

/* nucleotide character to kmer */
kmer_t chartokmer(char);

/* nucleotide string to kmer */
kmer_t strtokmer(const char*);

/* get a particular nucleotide from a sequence */
kmer_t kmer_get_nt(kmer_t* x, size_t i);

/* complement */
kmer_t kmer_comp(kmer_t);

/* reverse complement */
kmer_t kmer_revcomp(kmer_t);

/* canonical */
kmer_t kmer_canonical(kmer_t);

/* hash functions for kmers */
uint64_t kmer_hash(kmer_t);
uint64_t kmer_hash_with_seed(kmer_t, uint64_t seed);


#endif


