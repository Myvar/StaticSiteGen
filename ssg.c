#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

// Simple error-handling macro
#define ERROR_EXIT(msg)                                                        \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

// Dynamic string builder using stb_ds
typedef struct {
  char *buf;
} StringBuilder;
static void sb_init(StringBuilder *sb) { sb->buf = NULL; }
static void sb_append_char(StringBuilder *sb, char c) { arrput(sb->buf, c); }
static void sb_append_str(StringBuilder *sb, const char *s) {
  while (*s)
    sb_append_char(sb, *s++);
}
static void sb_finalize(StringBuilder *sb) { arrput(sb->buf, '\0'); }

// Helper to process inline Org-mode links ([[target]] or [[target][desc]])
static void process_inline_links(StringBuilder *sb, const char *text) {
  const char *p = text;
  while (1) {
    const char *start = strstr(p, "[[");
    if (!start) {
      sb_append_str(sb, p);
      break;
    }
    for (const char *q = p; q < start; ++q)
      sb_append_char(sb, *q);
    const char *end = strstr(start + 2, "]]");
    if (!end) {
      sb_append_str(sb, start);
      break;
    }
    const char *sep = strstr(start + 2, "][");
    char target[PATH_MAX], desc[PATH_MAX];
    if (sep && sep < end) {
      size_t tlen = sep - (start + 2);
      size_t dlen = end - (sep + 2);
      memcpy(target, start + 2, tlen);
      target[tlen] = '\0';
      memcpy(desc, sep + 2, dlen);
      desc[dlen] = '\0';
    } else {
      size_t tlen = end - (start + 2);
      memcpy(target, start + 2, tlen);
      target[tlen] = '\0';
      const char *base = strrchr(target, '/');
      base = base ? base + 1 : target;
      strncpy(desc, base, PATH_MAX - 1);
      desc[PATH_MAX - 1] = '\0';
      size_t dlen2 = strlen(desc);
      if (dlen2 > 4 && strcmp(desc + dlen2 - 4, ".org") == 0)
        desc[dlen2 - 4] = '\0';
    }
    size_t t2 = strlen(target);
    if (t2 > 4 && strcmp(target + t2 - 4, ".org") == 0)
      target[t2 - 4] = '\0';
    sb_append_str(sb, "<a href=\"");
    sb_append_str(sb, target);
    sb_append_str(sb, ".html\">");
    sb_append_str(sb, desc);
    sb_append_str(sb, "</a>");
    p = end + 2;
  }
}

// HTML file structure
typedef struct {
  StringBuilder content;
  char title[64];
  char author[64];
  char path[PATH_MAX];
} HtmlFile;

// Site with dynamic array of files
typedef struct {
  HtmlFile *files;
} StaticSite;

// Recursively ensure directory exists
bool ensure_directory(const char *file_path) {
  char copy[PATH_MAX];
  strncpy(copy, file_path, PATH_MAX - 1);
  copy[PATH_MAX - 1] = '\0';
  char *dir = dirname(copy);
  struct stat st;
  if (stat(dir, &st) == -1) {
    if (errno == ENOENT) {
      if (!ensure_directory(dir))
        return false;
      if (mkdir(dir, 0755) == -1) {
        perror("mkdir");
        return false;
      }
    } else {
      perror("stat");
      return false;
    }
  } else if (!S_ISDIR(st.st_mode)) {
    fprintf(stderr, "%s is not a directory\n", dir);
    return false;
  }
  return true;
}

// Read entire file into memory
bool read_file(const char *path, char **buf_out, size_t *size_out) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return false;
  }
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buf = malloc(sz + 1);
  if (!buf) {
    fclose(f);
    return false;
  }
  if (fread(buf, 1, sz, f) != (size_t)sz) {
    free(buf);
    fclose(f);
    return false;
  }
  buf[sz] = '\0';
  fclose(f);
  *buf_out = buf;
  *size_out = sz;
  return true;
}

// Convert Org file to HTML
bool convert_org_to_html(const char *src, HtmlFile *html) {
  char *text = NULL;
  size_t sz = 0;
  if (!read_file(src, &text, &sz)) {
    fprintf(stderr, "Cannot read %s\n", src);
    return false;
  }

  sb_init(&html->content);
  html->title[0] = html->author[0] = '\0';
  sb_append_str(&html->content, "<html>\n<head>\n");

  bool in_list = false;
  bool found_header = false;
  char *saveptr;
  char *line = strtok_r(text, "\n", &saveptr);
  while (line) {
    // trim leading whitespace for structural checks
    char *trim = line;
    while (*trim && isspace((unsigned char)*trim))
      trim++;

    if (strncmp(trim, "#+title:", 8) == 0) {
      strncpy(html->title, trim + 8, sizeof(html->title) - 1);
      found_header = true;
      sb_append_str(&html->content, "<title>");
      sb_append_str(&html->content, html->title);
      sb_append_str(&html->content, "</title>\n");
      sb_append_str(&html->content, "</head>\n");
      sb_append_str(&html->content, "<body>\n");
    } else if (strncmp(trim, "#+author:", 9) == 0) {
      strncpy(html->author, trim + 9, sizeof(html->author) - 1);
      found_header = true;
    } else if (*trim == '*') {
      if (!found_header) {
        found_header = true;
        sb_append_str(&html->content, "</head>\n");
        sb_append_str(&html->content, "<body>\n");
      }
      if (in_list) {
        sb_append_str(&html->content, "</ul>\n");
        in_list = false;
      }
      int depth = 0;
      while (trim[depth] == '*')
        depth++;
      const char *content = trim + depth;
      while (*content && isspace((unsigned char)*content))
        content++;
      char tag[8];
      snprintf(tag, sizeof(tag), "h%d", depth);
      sb_append_str(&html->content, "<");
      sb_append_str(&html->content, tag);
      sb_append_str(&html->content, ">");
      process_inline_links(&html->content, content);
      sb_append_str(&html->content, "</");
      sb_append_str(&html->content, tag);
      sb_append_str(&html->content, ">\n");
    } else if (*trim == '-') {
      if (!found_header) {
        found_header = true;
        sb_append_str(&html->content, "</head>\n");
        sb_append_str(&html->content, "<body>\n");
      }
      if (!in_list) {
        sb_append_str(&html->content, "<ul>\n");
        in_list = true;
      }
      sb_append_str(&html->content, "  <li>");
      char *item = trim + 1;
      while (*item && isspace((unsigned char)*item))
        item++;
      process_inline_links(&html->content, item);
      sb_append_str(&html->content, "</li>\n");
    } else {
      if (!found_header) {
        found_header = true;
        sb_append_str(&html->content, "</head>\n");
        sb_append_str(&html->content, "<body>\n");
      }
      if (in_list) {
        sb_append_str(&html->content, "</ul>\n");
        in_list = false;
      }
      sb_append_str(&html->content, "<pre>");
      process_inline_links(&html->content, trim);
      sb_append_str(&html->content, "</pre>\n");
    }
    line = strtok_r(NULL, "\n", &saveptr);
  }
  if (in_list)
    sb_append_str(&html->content, "</ul>\n");
  sb_append_str(&html->content, "</body>\n</html>\n");
  sb_finalize(&html->content);
  free(text);
  return true;
}

// Recursively traverse and convert
void traverse_dir(const char *root, const char *path, StaticSite *site) {
  DIR *dp = opendir(path);
  if (!dp)
    ERROR_EXIT("opendir");
  struct dirent *e;
  while ((e = readdir(dp))) {
    if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
      continue;
    char full[PATH_MAX];
    snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
    if (e->d_type == DT_DIR)
      traverse_dir(root, full, site);
    else if (strstr(e->d_name, ".org")) {
      HtmlFile f = {0};
      const char *rel = full + strlen(root);
      size_t l = strlen(rel);
      if (l > 4 && !strcmp(rel + l - 4, ".org"))
        l -= 4;
      snprintf(f.path, sizeof(f.path), "%.*s", (int)l, rel);
      if (!convert_org_to_html(full, &f))
        ERROR_EXIT("convert_org_to_html");
      arrput(site->files, f);
    }
  }
  closedir(dp);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <org_root_dir>\n", argv[0]);
    return EXIT_FAILURE;
  }
  StaticSite site = {0};
  traverse_dir(argv[1], argv[1], &site);
  for (int i = 0; i < arrlen(site.files); i++) {
    HtmlFile *hf = &site.files[i];
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "./out/%s.html", hf->path);
    if (!ensure_directory(out))
      ERROR_EXIT("ensure_directory");
    FILE *f = fopen(out, "w");
    if (!f)
      ERROR_EXIT("fopen");
    if (fputs(hf->content.buf, f) == EOF)
      ERROR_EXIT("fputs");
    fclose(f);
  }
  return EXIT_SUCCESS;
}
