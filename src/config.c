#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LEN 256

typedef enum { SECTION_NONE, SECTION_GENERAL, SECTION_VISIBILITY, SECTION_SORT } Section;

static char *trim(char *str) {
  char *end;
  while (isspace((unsigned char)*str))
    str++;
  if (*str == '\0')
    return str;
  end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end))
    end--;
  *(end + 1) = '\0';
  return str;
}

void insert_or_update_app(AppList *list, const char *app_id, int value) {
  AppNode *current = list->head;

  // Check if the item already exists to update it inline
  while (current != NULL) {
    if (strcasecmp(current->app_id, app_id) == 0) {
      current->value = value;
      return;
    }
    current = current->next;
  }

  // Create a new node if it wasn't found
  AppNode *new_node = malloc(sizeof(AppNode));
  if (!new_node)
    return;

  strncpy(new_node->app_id, app_id, sizeof(new_node->app_id) - 1);
  new_node->app_id[sizeof(new_node->app_id) - 1] = '\0';
  new_node->value = value;
  new_node->next = NULL;

  // Attach to tail efficiently
  if (list->tail == NULL) {
    list->head = new_node;
    list->tail = new_node;
  } else {
    list->tail->next = new_node;
    list->tail = new_node;
  }
}

// In-place sorting of the linked list based on the sort value (Ascending)
// Re-evaluates and updates both head and tail pointers when complete
void sort_app_list(AppList *list) {
  if (list->head == NULL || list->head->next == NULL)
    return;

  AppNode *sorted = NULL;
  AppNode *current = list->head;
  AppNode *new_tail = NULL;

  while (current != NULL) {
    AppNode *next = current->next;

    // Insert 'current' into the temporary 'sorted' list
    if (sorted == NULL || sorted->value >= current->value) {
      current->next = sorted;
      sorted = current;
      if (new_tail == NULL) {
        new_tail = current; // The first item inserted at the head becomes the tail
      }
    } else {
      AppNode *search = sorted;
      while (search->next != NULL && search->next->value < current->value) {
        search = search->next;
      }
      current->next = search->next;
      search->next = current;
      if (current->next == NULL) {
        new_tail = current; // Track the new tail if pushed to the absolute end
      }
    }
    current = next;
  }

  list->head = sorted;
  list->tail = new_tail;
}

int load_config(const char *filename, AppConfig *config) {
  FILE *file = fopen(filename, "r");
  if (!file)
    return -1;

  memset(config, 0, sizeof(AppConfig));
  char line[MAX_LINE_LEN];
  Section current_section = SECTION_NONE;

  while (fgets(line, sizeof(line), file)) {
    char *trimmed = trim(line);
    if (trimmed[0] == '\0' || trimmed[0] == '#' || trimmed[0] == ';')
      continue;

    if (trimmed[0] == '[' && trimmed[strlen(trimmed) - 1] == ']') {
      trimmed[strlen(trimmed) - 1] = '\0';
      char *section_name = trim(trimmed + 1);

      if (strcasecmp(section_name, "general") == 0)
        current_section = SECTION_GENERAL;
      else if (strcasecmp(section_name, "visibility") == 0)
        current_section = SECTION_VISIBILITY;
      else if (strcasecmp(section_name, "sort") == 0)
        current_section = SECTION_SORT;
      else
        current_section = SECTION_NONE;
      continue;
    }

    char *eq = strchr(trimmed, '=');
    if (eq) {
      *eq = '\0';
      char *key = trim(trimmed);
      char *value_str = trim(eq + 1);
      int value = atoi(value_str);

      switch (current_section) {
      case SECTION_GENERAL:
        if (strcasecmp(key, "debug") == 0)
          config->general.debug = value;
        else if (strcasecmp(key, "oneshot") == 0)
          config->general.oneshot = value;
        else if (strcasecmp(key, "quiet") == 0)
          config->general.quiet = value;
        break;
      case SECTION_VISIBILITY:
        insert_or_update_app(&config->visibility, key, value);
        break;
      case SECTION_SORT:
        insert_or_update_app(&config->sort, key, value);
        break;
      default:
        break;
      }
    }
  }
  fclose(file);
  sort_app_list(&config->sort);
  return 0;
}

int save_config(const char *filename, const AppConfig *config) {
  FILE *file = fopen(filename, "w");
  if (!file) {
    perror("Error opening config file for writing");
    return -1;
  }

  // Write general section
  fprintf(file, "# ShellUI Locker configuration\n\n");
  fprintf(file, "[general]\n");
  fprintf(file, "debug=%d\n", config->general.debug);
  fprintf(file, "# set to 1 to probe, apply and exit\n");
  fprintf(file, "oneshot=%d\n", config->general.oneshot);
  fprintf(file, "# set to 1 to disable popup notifications\n");
  fprintf(file, "quiet=%d\n\n", config->general.quiet);

  // Write visibility section
  fprintf(file, "# Icon visibility, same for each user\n");
  fprintf(file, "[visibility]\n");
  for (AppNode *curr = config->visibility.head; curr != NULL; curr = curr->next) {
    fprintf(file, "%s=%d\n", curr->app_id, curr->value);
  }
  fprintf(file, "\n");

  // Write sort section (this prints keys in their new sorted numerical order)
  fprintf(file, "# This allows to 'pin' app icons to their specific spot in the corresponding\n");
  fprintf(file, "# tab. The smaller the number, the more to the left it is on screen in the tab.\n");
  fprintf(file, "# The second icon in the tab is usually the default one when you switch to that\n");
  fprintf(file, "# tab, so Welcome for games, and Y2JB for Media, in this example.\n");
  fprintf(file, "# All icons that aren't specified here will use their builtin sort values and\n");
  fprintf(file, "# will be sorted by PS5 ShellUI based on their last access time.\n");
  fprintf(file, "[sort]\n");
  for (AppNode *curr = config->sort.head; curr != NULL; curr = curr->next) {
    fprintf(file, "%s=%d\n", curr->app_id, curr->value);
  }

  fclose(file);
  return 0;
}

void free_list(AppList *list) {
  AppNode *curr = list->head;
  while (curr) {
    AppNode *tmp = curr->next;
    free(curr);
    curr = tmp;
  }
  list->head = NULL;
  list->tail = NULL;
}

void free_config(AppConfig *config) {
  free_list(&config->visibility);
  free_list(&config->sort);
}
