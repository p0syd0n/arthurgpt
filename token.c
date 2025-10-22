#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "hashtable_int.c"
#include "config.h"


char** vocabulary; // Heap
int vocabulary_index; // Stack

Hashtable* vocabulary_hashtable; // Stack

typedef struct Token {
    char* token_in_pool;
    size_t len;
} Token; // Heap

Token* corpus; // Heap

size_t corpus_size; // Stack
size_t corpus_non_null_elements; // Stack
size_t null_count = 0;
char* pool; // Heap
size_t pool_offset; // Stack

char* pool_strdup(const char* s) {
    size_t len = strlen(s) + 1;
    char* dest = pool + pool_offset;
    memcpy(dest, s, len);
    pool_offset += len;
    return dest;
}

int tokenize_chars(char* characters, int size) {
    Hashtable* potential_tokens = create_hashtable(HASHTABLE_SIZE);
    // The most common token (character combination)
    char max_token[3];
    // The amount of times the max_token appears in the corpus.
    int appearances = 0;
    // The temporary variable for creating a string, to add each character to the vocabulary array and hashmap
    char* temp;
    // 1. Per-character vocabulary
    for (int i = 0; i < size; i++) {
        char temp[2] ;
        temp[0] = characters[i];
        temp[1] = '\0';
        // Use a hashtable because it is faster than looping over the vocabulary list and looking to see if we have the character yet.
        int* result = table_get(vocabulary_hashtable, temp);
        if (result == NULL) {
            //printf("%c does not exist in the vocabulary. Adding which is %s\n", characters[i], temp);
            table_add(vocabulary_hashtable, temp, 1);
            //printf("Added, key %s now has value %d.\n", temp, *table_get(vocabulary_hashtable, temp));
            vocabulary[vocabulary_index++] = pool_strdup(temp);
        }
    }

    printf("Added all characters to vocabulary.\n");

    // 2. Per pair frequencies (BPE merge step)
    for (int i = 0; i < size - 1; i++) {
        // The potential token
        char together[3] = { characters[i], characters[i+1], '\0' };
        // Do we have it yet?
        int* result = table_get(potential_tokens, together);
        // fr_result stores the number of times it exists in the corpus, on the stack
        int fr_result;
        if (result == NULL) {
            // We don't have it. Mark it down in the hashtable.
            table_add(potential_tokens, together, 1);
            fr_result = 1;
        } else {
            // We have it. Increment the value. 
            fr_result = (*result)+1;
            table_add(potential_tokens, together, (*result)+1);
        }
        //printf("The token has been seen %d times.\n", fr_result);

        // Have we beat the record?
        if (fr_result > appearances) {
            appearances = fr_result;
            memcpy(max_token, together, 2);
            max_token[2] = '\0';
        }

        // Check for shit like aaa . This is 0 potential tokens
        if (i+2 < size && characters[i] == characters[i+1] && characters[i+2] == characters[i]) {
            i++;
        }
    }
    printf("Counted potential tokens.\n");

    // I stand in support of hashtable abolitionism.
    free_hashtable(potential_tokens);
    // Ugh, strduping. TODO: pool for vocabulary as well.
    vocabulary[vocabulary_index++] = pool_strdup(max_token);


    // We substract now and begin building the tokens_strings list, which is a stringified list of characters and the token we just decided on.
    // TODO: Go straight from the max_token + characters[] to corpus updating. This intermediary step is unnecessary.
    size -= appearances;

    char** tokens_strings = calloc(size, sizeof(char*));
    int tokens_strings_index = 0;
    char together[3];

    for (int i = 0; tokens_strings_index < size; i++) {
        
        together[0] = characters[i];
        together[1] = characters[i+1];
        together[2] = '\0';

        // If we have the right one, add it to that little token list we have, and make sure we don't check the next character.
        // Otherwise, just add the current character.
        if (strcmp(together, max_token) == 0) {
            tokens_strings[tokens_strings_index++] = pool_strdup(max_token);
            i++;
            continue;
        }

        tokens_strings[tokens_strings_index++] = strndup(&characters[i], 1);
    }

    printf("Added all tokens + characters to a string list of new tokens (TODO: go straight to corpus.).\n");

    // If the last two characters are a token, the last character will be skipped when checking, because of the i++ - we do not want to check the end of the new token we have already added.
    // However, if the last two characters are not a token, the last character will be skipped. I do not do anything ahout that, despite it being one line. TODO: do this.

    for (int i = 0; i < size; i++) {
        // The memory is already alloc'd. All we have to do is fill it.
        corpus[i].token_in_pool = pool_strdup(tokens_strings[i]);
        corpus[i].len = strlen(tokens_strings[i]);
    }
    printf("Added all tokens to the corpus.\n");

    // These are the same at this point.
    corpus_size = size;
    corpus_non_null_elements = size;

    return 0;
}

int tokenize_strings() {
    Hashtable* potential_tokens = create_hashtable(corpus_size);

    char max_token[MAX_TOKEN_SIZE]; // Give it enough space to fit any possible token
    int appearances = 0; // How many times it appears.
    size_t max_token_length; // How long is it? To not do strlen later.

    for (int i = 0; i < corpus_size-1; i++) {
        // If we hit a NULL one, go on to the next.
        if (corpus[i].token_in_pool == NULL) continue;
        // We are not null.
        Token* this_token = &(corpus[i]);

        // The next token could be NULL. So, we declare it and roll up our sleeves.
        Token* next_token;
        // Find the next token in corpus who is not NULL, and keep track of how far away it is.
        size_t additions_to_next_valid_token = 0; // We need to know how far it is because we will jump to it later, to skip the NULL values.
        while (i + 1 + additions_to_next_valid_token < corpus_size &&
            corpus[i + 1 + additions_to_next_valid_token].token_in_pool == NULL) { // As long as we are in bound, go to the next token.
            additions_to_next_valid_token++;
        }
        if (i + 1 + additions_to_next_valid_token >= corpus_size) { // We went out of bound just now in the previous loop.
            // no next valid token exists
            break;
        }
        // We didn't break out - we found the token. Use it.
        next_token = &(corpus[i + 1 + additions_to_next_valid_token]);
        // Set the iteration variable now. We won't be using it again.
        i += additions_to_next_valid_token;

       // printf("Okay, we have this and the next non-null token. %s, %s\n", this_token->token_in_pool, next_token->token_in_pool);

       // Get the two tokens together.
        size_t len1 = this_token->len;
        size_t len2 = next_token->len;
        char sequence[len1 + len2 + 1];
       // printf("Allocated %d space for the sequence.\n", len1+len2);
        memcpy(sequence, this_token->token_in_pool, len1);
        memcpy(sequence + len1, next_token->token_in_pool, len2);
        sequence[len1 + len2] = '\0';

       // printf("Thjje sequence checking now is %s\n", sequence);

        // Do we have it?
        int* result = table_get(potential_tokens, sequence);
        int fr_result;
        if (result == NULL) {
            // No. Add it
            table_add(potential_tokens, sequence, 1);
            fr_result = 1;
        } else {
            // Yes. Increment it
            fr_result = (*result)+1;
            table_add(potential_tokens, sequence, (*result)+1);
        }

        // Did it beat the record?
        if (fr_result > appearances) {
            max_token_length = len1 + len2;
            memcpy(max_token, sequence, len1+len2);
            max_token[len1+len2] = '\0';
            appearances = fr_result;
        }


        

        // if i have a situation like aaab , i want to have 1 potential token of aa, not 2. (If it gets chosen, I can only make one token aa out of it.) So if we just checked a | aab , skip to aaa | b (To the next next non-null token)
        // Make sure that the next two tokens are the same length, because if they're not then our edge condition isn't possible
        // Also check that the tokens aren't null (already)
        
        // Ensure we have enough elements ahead
        if (i + 2 < corpus_size) {
            // Find the next valid token after next_token. We did this process earlier.
            Token* next_next_token = NULL;
            size_t distance_to_next_next_token = 0;

            while (i + 2 + distance_to_next_next_token < corpus_size &&
                corpus[i + 2 + distance_to_next_next_token].token_in_pool == NULL) {
                distance_to_next_next_token++;
            }

            
            // Only proceed if it is in the bounds (we found it)
            if (i + 2 + distance_to_next_next_token < corpus_size) {
                next_next_token = &corpus[i + 2 + distance_to_next_next_token];

                // Only do this check if the lengths are equal. Tokens can't be the same if their lengths aren't the same.
                if (next_token->len == next_next_token->len) {
                    size_t next_tokens_length = next_token->len + next_next_token->len;

                    // Now check if they are the same, if they are, proceed to the furthest token
                    char next[next_tokens_length + 1];
                    memcpy(next, next_token->token_in_pool, next_token->len);
                    memcpy(next + next_token->len, next_next_token->token_in_pool, next_next_token->len);
                    next[next_tokens_length] = '\0';

                    if (memcmp(sequence, next, next_tokens_length) == 0) {
                        i += distance_to_next_next_token;
                    }
                }
            }
        }
   
    }



    free_hashtable(potential_tokens);
    
    // Appearances == 1 means no combination appeared more than once.
    if (appearances == 1) {
        printf("NO MORE TOKENIZATION POSSIBLE.\n");
        return -1;
    }

  //  printf("vocab size: %d\n", vocabulary_index+1);

   // printf("Most common TOken is %s with %d appearances.\n", max_token, appearances);

   // Apply lazy compaction. Compactment? hmm.

    for (int i = 0; i < corpus_size-1; i++) {
        // We have to repeat the shenanigans regarding finding the next non-null token.
        if (corpus[i].token_in_pool == NULL) continue;
        Token* this_token = &(corpus[i]);
        Token* next_token;
        size_t distance_to_next_token = 0;
        while(corpus[i+1+distance_to_next_token].token_in_pool == NULL && i + 1 + distance_to_next_token < corpus_size) {
            distance_to_next_token++;
        }

        if (i+1+distance_to_next_token >= corpus_size) {
            break;
        }
        // Shenanigans over.

        next_token = &(corpus[i+1+distance_to_next_token]);

        size_t next_two_size = this_token->len + next_token->len;
        // If what we have isn't the same length as the max_token, they certainly aren't equivalent.
        if (next_two_size != max_token_length) continue;
        char next_two[next_two_size];
        memcpy(next_two, this_token->token_in_pool, this_token->len);
        memcpy(next_two + this_token->len, next_token->token_in_pool, next_token->len);

        if (memcmp(next_two, max_token, max_token_length) == 0) {
           // printf("Okay,setting %s to NULL\n", next_token->token_in_pool);

            // We stumbled across the new token.
            // Nullify the second token. It doesn't exist anymore. 
            // Heres the kicker: we don't move elements down. The NULL element will just stay in the corpus.
            // That's why we have been jumping through hoops to make sure our tokens are non-NULL.
            // Why?
            // Because the alternative would be iterating through the entire array here, and shifting each element down the required number of spaces, depending on appearances.
            // This is an O(n) operation - it has to loop through the whole array.
            // Finding the next non-NULL element will not loop through the whole array - it will loop through the array until it finds the next non-NULL element, which is likely much closer than the end of the array. In any case, its less iterations than really deleting.
            // TODO: keep track of how many NULL values exist. Shrink the corpus each time a threshold is passed. Move the corpus to the stack, once it is small enough?
            next_token->token_in_pool = NULL;
            null_count++;
            this_token->token_in_pool = pool_strdup(max_token);
            this_token->len = max_token_length;
            // Jump to the next token. This will jump to the token we just deleted, which is NULL, so the loop will slide to the next non-NULL element.
            i+=distance_to_next_token;
            continue;
        }
        if (null_count > ACCEPTABLE_NULL_CONTENT*corpus_size) {
            size_t write_index = 0;
            for (size_t read_index = 0; read_index < corpus_size; read_index++) {
                if (corpus[read_index].token_in_pool != NULL) {
                    if (write_index != read_index) {
                        corpus[write_index] = corpus[read_index];
                    }
                    write_index++;
                }
            }
            // Clear the tail
            for (size_t i = write_index; i < corpus_size; i++) {
                corpus[i].token_in_pool = NULL;
                corpus[i].len = 0;
            }

            corpus_size = write_index;
            corpus_non_null_elements = write_index;
            null_count = 0;
            printf("Compacted corpus: %zu active tokens remain.\n", corpus_size);
        }
    }

    // Add it to the vocab hashtable. Is this really necessary? TODO: is this really necessary?
    //table_add(vocabulary_hashtable, max_token, 1);
    // TODO: also have a pool of memory for vocabulary
    vocabulary[vocabulary_index++] = pool_strdup(max_token);

    // YEah
    corpus_non_null_elements -= appearances;

    return 0;
}

int begin() {
    // starting here
    int fd = open("text.txt", O_RDONLY);
    struct stat st;
    fstat(fd, &st);
    size_t filesize = st.st_size;

    char* data = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) { perror("mmap"); exit(1); }
    printf("opened file\n");

    // ending here
    // We open up a file that will load into memory as we read it.

    vocabulary = malloc(DESIRED_VOCABULARY_SIZE*sizeof(char*));
    vocabulary_index = 0; // Where we will insert the vocab word. vocab count is this + 1
    vocabulary_hashtable = create_hashtable(DESIRED_VOCABULARY_SIZE); // Prevent duplicates without O(N) search through the vocabulary array

    // Go for a swim

    // The pool is a nice optimization. idk if it really adds any performance.
    // Instead of having memory all over the place, we can have all of the corpus data in the same place. The pool_strdup function adds to it and returns a pointer to the spot it was added.
    // Token 's point to spots inside this pool. 
    // Since the data is all in the same place, CPU caching magic makes things faster.
    pool = malloc(POOL_SIZE);
    pool_offset = 0;
    
    // Heap allocated. What, you didnt believe me?
    corpus = malloc(sizeof(Token)*MAX_CORPUS_SIZE);

    // Call this once. It is (will be) optimized for characters
    tokenize_chars(data, filesize);
    printf("Tokenized characters.\n");
    
    size_t i = 0; // Keep track for debugging.
    // tokenize until we hit the vocabulary limit or until we can't anymore.
    while (tokenize_strings() != -1 && vocabulary_index+1 < DESIRED_VOCABULARY_SIZE) { printf("Tokenization iteration %d. %d vocabulary words. %d active corpus size.\n", ++i, vocabulary_index+1, corpus_non_null_elements);}

    printf("Tokenized strings.\n");

    for (int i = 0; i < vocabulary_index; i++) {
      printf("Vocabulary item: %s\n", vocabulary[i]);
    }
    printf("%d bytes tokenized\n", filesize);

    munmap(data, filesize);
    close(fd);
    return 0;
}


int main() {
    begin();
    return 0;
}


/*
TODO:
- convert to while loops instead of for loops
- Figure out a theoretical upper limit on the tokenized corpus size




*/