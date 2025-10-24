#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

#include "utils/hash_utils.c"
#include "utils/hashtable_u64_int.c"
#include "utils/hashtable_u64_str.c"

#include "config.h"

typedef struct Token {
    char* token_in_pool;
    size_t len;
    uint64_t hash;
} Token;

Token* vocabulary;
int vocabulary_index;

HashU64Int* vocabulary_hashtable;
HashU64Int* tokenpair_frequency;
HashU64Str* int_vocabulary_hashtable;

Token* corpus;
Token* temp_corpus;  // Temporary buffer for compaction
size_t corpus_size;

char* pool;
size_t pool_offset;

char* pool_memcpy(const char* s, size_t len) {
    char* dest = pool + pool_offset;
    memcpy(dest, s, len);
    dest[len] = '\0';
    pool_offset += len + 1;
    return dest;
}

int tokenize_chars(char* characters, int size) {
    uint64_t max_token_hash;
    size_t appearances = 0;
    char max_token[2];

    for (int i = 0; i < size; i++) {
        uint64_t character_hash = hash_bytes(&(characters[i]), 1);
        int* result = get_u64_int(vocabulary_hashtable, character_hash);
        char* character_pooled = pool_memcpy(&(characters[i]), 1);
        corpus[i].hash = character_hash;
        corpus[i].token_in_pool = character_pooled;
        corpus[i].len = 1;

        if (result == NULL) {
            vocabulary[vocabulary_index].token_in_pool = character_pooled;
            vocabulary[vocabulary_index].len = 1;
            vocabulary[vocabulary_index++].hash = character_hash;
            set_u64_int(vocabulary_hashtable, character_hash, 1);
        }
    }

    for (int i = 0; i < size-1; i++) {
        uint64_t potential_token = mix(corpus[i].hash, corpus[i+1].hash);
        int* result = get_u64_int(tokenpair_frequency, potential_token);
        int character_frequency_thus_far = result ? (*result)+1 : 1;
        set_u64_int(tokenpair_frequency, potential_token, character_frequency_thus_far);

        if (character_frequency_thus_far > appearances) {
            appearances = character_frequency_thus_far;
            max_token_hash = potential_token;
            max_token[0] = characters[i];
            max_token[1] = characters[i+1];
        }

        if (i+2 < size && characters[i] == characters[i+1] && characters[i+2] == characters[i]) {
            i++;
        }
    }

    char* max_token_pool = pool_memcpy(max_token, 2);

    // Eagerly compact while merging
    size_t write_idx = 0;
    for (int i = 0; i < size-1; i++) {
        if (mix(corpus[i].hash, corpus[i+1].hash) == max_token_hash) {
            temp_corpus[write_idx].hash = max_token_hash;
            temp_corpus[write_idx].token_in_pool = max_token_pool;
            temp_corpus[write_idx].len = 2;
            write_idx++;
            i++;  // Skip the next token
        } else {
            temp_corpus[write_idx] = corpus[i];
            write_idx++;
        }
    }
    // Handle last token if it wasn't merged
    if (size > 0 && (size == 1 || mix(corpus[size-2].hash, corpus[size-1].hash) != max_token_hash)) {
        temp_corpus[write_idx++] = corpus[size-1];
    }

    // Swap buffers
    Token* swap = corpus;
    corpus = temp_corpus;
    temp_corpus = swap;
    corpus_size = write_idx;

    vocabulary[vocabulary_index].token_in_pool = max_token_pool;
    vocabulary[vocabulary_index].len = 2;
    vocabulary[vocabulary_index++].hash = max_token_hash;

    return 0;
}

int tokenize_strings() {
    clear_u64_int(tokenpair_frequency);

    char max_token[MAX_TOKEN_SIZE];
    uint64_t max_token_hash = 0;
    int appearances = 0;
    size_t max_token_length = 0;

    // Count frequencies - simple sequential scan, no NULLs to skip
    for (size_t i = 0; i < corpus_size - 1; i++) {
        Token* this_token = &corpus[i];
        Token* next_token = &corpus[i + 1];

        uint64_t potential_token_hash = mix(this_token->hash, next_token->hash);
        int* fr = get_u64_int(tokenpair_frequency, potential_token_hash);
        int fr_result = fr ? (*fr + 1) : 1;
        set_u64_int(tokenpair_frequency, potential_token_hash, fr_result);

        if (fr_result > appearances) {
            appearances = fr_result;
            max_token_hash = potential_token_hash;
            memcpy(max_token, this_token->token_in_pool, this_token->len);
            memcpy(max_token + this_token->len, next_token->token_in_pool, next_token->len);
            max_token_length = this_token->len + next_token->len;
            max_token[max_token_length] = '\0';
        }
    }

    if (appearances <= 1) {
        printf("NO MORE TOKENIZATION POSSIBLE.\n");
        return -1;
    }

    char* max_token_pool = pool_memcpy(max_token, max_token_length);

    // Eagerly compact while merging - write directly to temp_corpus
    size_t write_idx = 0;
    for (size_t i = 0; i < corpus_size - 1; i++) {
        Token* this_token = &corpus[i];
        Token* next_token = &corpus[i + 1];

        if (this_token->len + next_token->len == max_token_length &&
            mix(this_token->hash, next_token->hash) == max_token_hash) {
            // Merge these two tokens
            temp_corpus[write_idx].token_in_pool = max_token_pool;
            temp_corpus[write_idx].len = max_token_length;
            temp_corpus[write_idx].hash = max_token_hash;
            write_idx++;
            i++;  // Skip the next token since we merged it
        } else {
            // Keep this token as-is
            temp_corpus[write_idx] = *this_token;
            write_idx++;
        }
    }

    // Handle last token if it wasn't merged
    if (corpus_size > 0) {
        Token* last = &corpus[corpus_size - 1];
        if (corpus_size == 1 || write_idx == 0 || temp_corpus[write_idx - 1].token_in_pool != max_token_pool ||
            corpus[corpus_size - 2].len + last->len != max_token_length) {
            temp_corpus[write_idx++] = *last;
        }
    }

    // Swap buffers
    Token* swap = corpus;
    corpus = temp_corpus;
    temp_corpus = swap;
    corpus_size = write_idx;

    set_u64_str(int_vocabulary_hashtable, max_token_hash, max_token);
    vocabulary[vocabulary_index].token_in_pool = max_token_pool;
    vocabulary[vocabulary_index].len = max_token_length;
    vocabulary[vocabulary_index++].hash = max_token_hash;

    return 0;
}

int begin() {
    int fd = open("text.txt", O_RDONLY);
    struct stat st;
    fstat(fd, &st);
    size_t filesize = st.st_size;

    char* data = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) { perror("mmap"); exit(1); }
    printf("opened file\n");

    vocabulary = malloc(DESIRED_VOCABULARY_SIZE * sizeof(Token));
    vocabulary_index = 0;
    vocabulary_hashtable = create_u64_int(DESIRED_VOCABULARY_SIZE);
    tokenpair_frequency = create_u64_int(HASHTABLE_SIZE);
    int_vocabulary_hashtable = create_u64_str(DESIRED_VOCABULARY_SIZE);

    pool = calloc(POOL_SIZE, 1);
    pool_offset = 0;

    // Allocate both corpus buffers
    corpus = malloc(sizeof(Token) * MAX_CORPUS_SIZE);
    temp_corpus = malloc(sizeof(Token) * MAX_CORPUS_SIZE);
    corpus_size = 0;

    printf("Starting tokenization\n");

    tokenize_chars(data, filesize);

    size_t i = 0;
    while (tokenize_strings() != -1 && vocabulary_index + 1 < DESIRED_VOCABULARY_SIZE) {
        // printf("Tokenization iteration %zu. %d vocabulary words. %zu corpus size.\n", ++i, vocabulary_index + 1, corpus_size);
    }

    printf("Tokenized strings.\n");
    printf("%zu bytes tokenized\n", filesize);

    munmap(data, filesize);
    close(fd);
    free(corpus);
    free(temp_corpus);
    return 0;
}

int main() {
    begin();
    return 0;
}