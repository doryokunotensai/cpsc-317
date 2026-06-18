#ifndef _BITVECTOR_H_
#define _BITVECTOR_H_
typedef uint64_t *bits;
#define SET_BIT(a, i)   (a[(i) >> 6] |=  (1 << ((i) & 63)))
#define CLEAR_BIT(a, i) (a[(i) >> 6] &= ~(1 << ((i) & 63)))
#define TEST_BIT(a, i)  (a[(i) >> 6] &   (1 << ((i) & 63)))
#define new_bit_vector(n) calloc((n + 63) / 64, sizeof(uint64_t))
#endif // _BITVECTOR_H_
