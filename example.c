#include <stdio.h>
#include "hashtable_int.c"

int main() {
  Hashtable* hashtable = create_hashtable(100000); // This will be enough to contain 100,000 seperate elements. However, it is also 800 kilobytes in ram.

  table_add(hashtable, "k", 3);
  table_add(hashtable, "beans", 4);


  int* value = table_get(hashtable, "k");
  int* value2 = table_get(hashtable, "beans");
  /*
  Do not free these. Otherwise, you will be left with dangling pointers. If you want to be able to free them, find the line in hashtable.c in table_get and make it say return strdup(node->value); and not return node->value; .  This will, however, make things slower. 
  */ 

  printf("Value: %d, %d\n", *value, *value2);

  int remove = table_remove(hashtable, "beans");
  int* value3 = table_get(hashtable, "beans");
  //printf("Value after being removed: %s\n", *value3); //  This will segfault because when you try to dereference it, itll be NULL

  free_hashtable(hashtable); // Pls free it. It wants freedom. I stand in support of hashtableian abolitionism
}
