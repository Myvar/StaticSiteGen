#include <dirent.h>
#include <errno.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

int ensure_root_directory_exists(const char *file_path) {
  char root_directory[256];
  strcpy(root_directory, file_path);

  char *last_slash = strrchr(root_directory, '/');
  if (last_slash != NULL) {
    *last_slash = '\0';
  }

  struct stat st;
  if (stat(root_directory, &st) == -1) {
    if (errno == ENOENT) {
      if (mkdir(root_directory, 0755) == -1) {
        perror("Error creating directory");
        return 0;
      }
    } else {
      perror("Error checking directory");
      return 0;
    }
  } else if (S_ISDIR(st.st_mode)) {
  } else {
    return 0;
  }

  return 1;
}

static int read_file(char *path, char **buffer, size_t *size) {

  FILE *f = fopen(path, "rb");
  if (!f)
    return false;

  fseek(f, 0, SEEK_END);
  size_t sz = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *buf = malloc(sz + 1);
  if (!buf) {
    fclose(f);
    return false;
  }

  fread(buf, 1, sz, f);
  buf[sz] = '\0';
  fclose(f);

  *buffer = buf;
  *size = sz;

  return true;
}

typedef struct HtmlFile {
  char *sb;
  char title[64];
  char author[64];
  char path[PATH_MAX];
} HtmlFile;

typedef struct SaticSite {
  HtmlFile *files;
} SaticSite;

int org_to_html_(char *path, HtmlFile *html) {
  char *buffer;
  size_t size;
  if (!read_file(path, &buffer, &size)) {
    printf("Please provide the org path\n");
    exit(EXIT_FAILURE);
  }

  /* Should be able to do it in a single loop? */
  int i = 0;
  for (i = 0; i < size; i++) {
    char c = buffer[i];
    if (c == '#') {
      char buf[64] = {0};
      char key[64] = {0};
      int start = 0;
      for (; i < size; i++) {
        c = buffer[i];
        if (c == ':') {
          start = 0;
          strcpy(key, buf);
          memset(buf, 0, 64);
          continue;
        } else if (c == '\n') {
          if (strcmp("#+title", key) == 0) {
            strcpy(html->title, key);
          } else if (strcmp("#+author", key) == 0) {
            strcpy(html->author, key);
          }
          break;
        }
        buf[start++] = c;
      }
    } else if (c == '*') {
      int depth = 1;
      int state = 0;
      for (; i < size; i++) {
        c = buffer[i];
        if (state == 0) {
          if (c == '*') {
            depth++;
          } else {
            state = 1;
            arrput(html->sb, '<');
            arrput(html->sb, 'h');
            arrput(html->sb, '0' + depth);
            arrput(html->sb, '>');
          }
        } else {
          if (c == '\n') {
            arrput(html->sb, '<');
            arrput(html->sb, '/');
            arrput(html->sb, 'h');
            arrput(html->sb, '0' + depth);
            arrput(html->sb, '>');
            break;
          } else {
            if (c == '[') {
              arrput(html->sb, '<');
              arrput(html->sb, 'a');
              char parts[6][PATH_MAX];
              int part = 0;
              int depth = 0;
              char buf[PATH_MAX] = {0};
              int start = 0;
              for (; i < size; i++) {
                c = buffer[i];
                if (c == '[') {
                  depth++;
                  continue;
                } else if (c == ']') {
                  strcpy(parts[part], buf);
                  start = 0;
                  part++;
                  depth--;
                  continue;
                } else if (depth == 0) {
                  if (part > 1) {
                    arrput(html->sb, ' ');
                    arrput(html->sb, 'h');
                    arrput(html->sb, 'r');
                    arrput(html->sb, 'e');
                    arrput(html->sb, 'f');
                    arrput(html->sb, '=');
                    arrput(html->sb, '"');
                    for (int j = 0; j < strlen(parts[0]) - 3; j++) {
                      arrput(html->sb, parts[0][j]);
                    }
                    arrput(html->sb, 'h');
                    arrput(html->sb, 't');
                    arrput(html->sb, 'm');
                    arrput(html->sb, 'l');
                    arrput(html->sb, '"');
                  }
                  arrput(html->sb, '>');
                  int idx = depth > 1 ? 0 : 1;
                  for (int j = 0; j < strlen(parts[idx]); j++) {
                    arrput(html->sb, parts[idx][j]);
                  }
                  arrput(html->sb, '<');
                  arrput(html->sb, '/');
                  arrput(html->sb, 'a');
                  arrput(html->sb, '>');
                  break;
                } else {
                  buf[start++] = c;
                }
              }
            }

            arrput(html->sb, c);
          }
        }
      }
    } else if (c == '[') {
      arrput(html->sb, '<');
      arrput(html->sb, 'a');
      char parts[6][PATH_MAX];
      int part = 0;
      int depth = 0;
      char buf[PATH_MAX] = {0};
      int start = 0;
      for (; i < size; i++) {
        c = buffer[i];
        if (c == '[') {
          depth++;
          continue;
        } else if (c == ']') {
          strcpy(parts[part], buf);
          start = 0;
          part++;
          depth--;
          continue;
        } else if (depth == 0) {
          if (part > 1) {
            arrput(html->sb, ' ');
            arrput(html->sb, 'h');
            arrput(html->sb, 'r');
            arrput(html->sb, 'e');
            arrput(html->sb, 'f');
            arrput(html->sb, '=');
            arrput(html->sb, '"');
            for (int j = 0; j < strlen(parts[0]) - 3; j++) {
              arrput(html->sb, parts[0][j]);
            }
            arrput(html->sb, 'h');
            arrput(html->sb, 't');
            arrput(html->sb, 'm');
            arrput(html->sb, 'l');
            arrput(html->sb, '"');
          }
          arrput(html->sb, '>');
          int idx = depth > 1 ? 0 : 1;
          for (int j = 0; j < strlen(parts[idx]); j++) {
            arrput(html->sb, parts[idx][j]);
          }
          arrput(html->sb, '<');
          arrput(html->sb, '/');
          arrput(html->sb, 'a');
          arrput(html->sb, '>');
          break;
        } else {
          buf[start++] = c;
        }
      }
    }
    arrput(html->sb, c);
  }
  arrput(html->sb, 0);
  return true;
}

void check_files(const char *dir_root, const char *dir_path, SaticSite *site) {
  struct dirent *entry;
  DIR *dp = opendir(dir_path);

  if (dp == NULL) {
    perror("opendir");
    return;
  }

  while ((entry = readdir(dp)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

    if (entry->d_type == DT_DIR) {
      check_files(dir_root, full_path, site);
    } else {
      if (strstr(entry->d_name, ".org") != NULL) {
        printf("Found .org file: %s\n", full_path + strlen(dir_root));
        HtmlFile html = {0};
        strcpy(html.path, full_path + strlen(dir_root));
        if (!org_to_html_(full_path, &html)) {
          printf("Failed to parse file\n");
          exit(EXIT_FAILURE);
        }

        arrput(site->files, html);
      }
    }
  }

  closedir(dp);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Please provide the org path\n");
    return EXIT_FAILURE;
  }

  SaticSite site = {0};
  check_files(argv[1], argv[1], &site);

  for (int i = 0; i < arrlen(site.files); i++) {
    HtmlFile hf = site.files[i];

    char path[PATH_MAX];
    strcpy(path, "./out");
    strcpy(path + strlen(path), hf.path);
    strcpy(path + (strlen(path) - 4), ".html");

    ensure_root_directory_exists(path);

    FILE *file = fopen(path, "w");
    if (file == NULL) {
      printf("path: %s\n", path);
      perror("Error opening file");
      return EXIT_FAILURE;
    }

    if (fputs(hf.sb, file) == EOF) {
      perror("Error writing to file");
    }

    fclose(file);
  }

  return EXIT_SUCCESS;
}
