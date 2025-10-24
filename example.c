#include <stdio.h>

#include "utils/hash_utils.c"
#include "utils/hashtable_u64_int.c"
#include "utils/hashtable_u64_str.c"

int main() {
    HashU64Int *counts = create_u64_int(4096);
    HashU64Str *reverse = create_u64_str(4096);

    uint64_t a = hash_str("a");
    uint64_t b = hash_str("b");
    uint64_t ab = mix(a, b);

    set_u64_int(counts, ab, 42);
    set_u64_str(reverse, ab, "ab");

    printf("Count[%lx] = %d\n", ab, *get_u64_int(counts, ab));
    printf("Reverse[%lx] = %s\n", ab, get_u64_str(reverse, ab));

    free_u64_int(counts);
    free_u64_str(reverse);
}
