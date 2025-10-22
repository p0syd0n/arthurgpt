#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// DJB2 hash function for strings
unsigned long hash_djb2(const char *str) {
    unsigned long hash = 5381;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

// Helper function to duplicate an int on the heap.
// Works like strdup but for ints.
int* intdup(int value) {
    int* p = malloc(sizeof(int)); // Allocate space for one int
    if (p != NULL) {
        *p = value; // Copy the value into heap space
    }
    return p;
}

typedef struct Node {
  char* key;       // Pointer to key string
  int* value;      // Pointer to value int (we own this, malloced by intdup)
  struct Node* next;     // Pointer to next item in linked list
  struct Node* previous; // Pointer to previous item in linked list
} Node;

typedef struct Hashtable {
  size_t size;    // Size, so we don't have to rely on a macro
  Node** table;   // Pointer to Node pointer array
} Hashtable;

// Make a hashtable of a given size
Hashtable* create_hashtable(size_t size) {
  Hashtable* hashtable = (Hashtable*)malloc(sizeof(Hashtable));

  Node** thetable = calloc(size, sizeof(Node*)); // Set all slots to NULL at the beginning
  hashtable->table = thetable;
  hashtable->size = size;

  return hashtable;
}

// Add a key/value pair to the hashtable
int table_add(Hashtable* hashtable, char* key, int value) {
  unsigned long hash = hash_djb2(key);
  size_t dictionary_index = hash % (hashtable->size); // Will always be in [0, size-1]

  Node* tempnode = (hashtable->table)[dictionary_index]; // Head of linked list for this slot
  
  if (tempnode == NULL) { // It's empty. Make a new node.
    (hashtable->table)[dictionary_index] = (Node*)malloc(sizeof(Node));
    tempnode = (hashtable->table)[dictionary_index];
    tempnode->key = strdup(key);         // Take a copy so user freeing won't break us
    tempnode->value = intdup(value);     // Same thing for value
    tempnode->next = NULL;               // No next one yet
    tempnode->previous = NULL;           // We are head, so no previous
    return 0;
  }

  // Otherwise, traverse the linked list
  while (tempnode != NULL) {
    // Update case: key already exists
    if (strcmp(tempnode->key, key) == 0) {
      free(tempnode->value);             // Free old heap int
      tempnode->value = intdup(value);   // Replace with new one
      return 0;
    }
    if (tempnode->next == NULL) break;   // End of list, key not found
    tempnode = tempnode->next;           // Move on
  }

  // Key not found: add at end of list
  Node* next_prep = (Node*)malloc(sizeof(Node));
  tempnode->next = next_prep;
  next_prep->key = strdup(key);          // Own our copy of the string
  next_prep->value = intdup(value);      // Own our copy of the int
  next_prep->previous = tempnode;
  next_prep->next = NULL;
  return 0;
}

// Remove a key/value pair from the hashtable
int table_remove(Hashtable* hashtable, char* key) {
  unsigned long hash = hash_djb2(key);
  size_t dictionary_index = hash % (hashtable->size);
  Node* tempnode = (hashtable->table)[dictionary_index];

  if (tempnode == NULL) { errno = ENOENT; return -1; } // Slot empty, key not found

  // Traverse linked list until we find the key
  while (strcmp(tempnode->key, key) != 0) {
    if (tempnode->next == NULL) { // End of list, not found
      errno = ENOENT;
      return -1;
    }
    tempnode = tempnode->next;
  }

  // Found it. Now handle different linked list cases.
  if (tempnode->previous == NULL && tempnode->next == NULL) {
    // Only node in this slot
    (hashtable->table)[dictionary_index] = NULL;

  } else if (tempnode->next != NULL && tempnode->previous != NULL) {
    // Node in the middle
    (tempnode->next)->previous = tempnode->previous;
    (tempnode->previous)->next = tempnode->next;

  } else if (tempnode->next != NULL && tempnode->previous == NULL) {
    // Head node, with something after it
    (hashtable->table)[dictionary_index] = tempnode->next;
    (tempnode->next)->previous = NULL;

  } else if (tempnode->next == NULL && tempnode->previous != NULL) {
    // Tail node, with something before it
    (tempnode->previous)->next = NULL;

  } else {
    return -1; // Should never happen
  }

  // Free this node and its owned data
  free(tempnode->key);
  free(tempnode->value);
  free(tempnode);

  return 0;
}

// Lookup a key in the hashtable
int* table_get(Hashtable* hashtable, char* key) {
  unsigned long hash = hash_djb2(key);
  size_t dictionary_index = hash % (hashtable->size);

  Node* tempnode = (hashtable->table)[dictionary_index];

  while (tempnode != NULL) {
    if (strcmp(tempnode->key, key) == 0) {
      return tempnode->value; // Return pointer to int (caller does not own this)
    }
    tempnode = tempnode->next;
  }

  errno = ENOENT; // Key not found
  return NULL;
}

// Free everything in the hashtable
void free_hashtable(Hashtable* hashtable) {
    for (size_t node_pointer_index = 0; node_pointer_index < hashtable->size; node_pointer_index++) {
        Node* node = (hashtable->table)[node_pointer_index];
        while (node) {
            Node* next = node->next;
            free(node->key);
            free(node->value);
            free(node);
            node = next;
        }
    }
    free(hashtable->table);
    free(hashtable);
}