//
//  main.c
//  docbuilder
//
//  Created by Marco Bambini on 12/04/23.
//


#define GENERATE_SQLITE_DATABASE    0
#define DOCBUILDER_VERSION          "0.3"

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <stdbool.h>
#include <sys/stat.h>
#if GENERATE_SQLITE_DATABASE
#include <sqlite3.h>
#endif
#include "cargs.h"

#define DIRREF                      DIR*
#define PATH_SEPARATOR              '/'

#define NEXT                        input[i++]
#define CURRENT                     input[i-1]
#define PEEK                        input[i]
#define PEEK2                       input[i+1]
#define NO_SKIP                     -1
#define SET_SKIP_N(_c,_n)           do {toskip = _c; nskip = _n;} while(0)
#define SET_SKIP(_c)                SET_SKIP_N(_c, 1)
#define SKIP_UNTIL(_c)              do{i++;} while(PEEK == _c)
#define RESET_SKIP()                do {toskip = NO_SKIP; nskip = 1;} while(0)

#define OPTIONS_COL                  json_mode && strip_astro_header

#if GENERATE_SQLITE_DATABASE
sqlite3 *db = NULL;
#endif
FILE *f = NULL;

const char *src_path = NULL;
const char *dest_path = NULL;
const char *base_url = NULL;
bool strip_html = false;
bool strip_jsx = false;
bool strip_md_title = false;
bool strip_astro_header = false;
bool use_transaction = false;
bool json_mode = false;
bool path_using_slug = false;
bool use_database = false;
bool create_db = false;

// MARK: - I/O Utils -

static bool is_directory (const char *path) {
    struct stat buf;
    
    if (lstat(path, &buf) < 0) return false;
    if (S_ISDIR(buf.st_mode)) return true;
    
    return false;
}

static char *directory_read (DIRREF ref) {
    if (ref == NULL) return NULL;
    
    while (1) {
        struct dirent *d;
        if ((d = readdir(ref)) == NULL) {
            closedir(ref);
            return NULL;
        }
        if (d->d_name[0] == '\0') continue;
        if (d->d_name[0] == '.') continue;
        return (char *)d->d_name;
    }
    return NULL;
}

static char *file_buildpath (const char *filename, const char *dirpath) {
    size_t len1 = (filename) ? strlen(filename) : 0;
    size_t len2 = (dirpath) ? strlen(dirpath) : 0;
    size_t len = len1 + len2 + 4;
    
    char *full_path = (char *)malloc(len);
    if (!full_path) return NULL;
    
    // check if PATH_SEPARATOR exists in dirpath
    if ((len2) && (dirpath[len2-1] != PATH_SEPARATOR))
        snprintf(full_path, len, "%s/%s", dirpath, filename);
    else
        snprintf(full_path, len, "%s%s", dirpath, filename);
    
    return full_path;
}

static char *file_buildurl (const char *base_url, const char *fullpath) {
    char *url = (char *)malloc(512);
    if (!url) return NULL;
    
    char *path = strdup(fullpath);
    if (!path) return NULL;
    
    char *p = (char *)path + strlen(src_path);
    if (p[0] == '/') ++p;
    
    char *index = strstr(p, "index.md");
    if (!index) index = strstr(p, "index.mdx");
    
    if (index) {
        // index page is different because it should be completely removed from the url
        size_t ilen = strlen(index);
        size_t plen = strlen(p);
        p[plen-ilen] = 0;
    } else {
        // any other page
        char *p2 = p;
        while (p2[0]) {
            if (p2[0] == '.') {
                p2[0] = 0;
                break;
            }
            ++p2;
        }
    }
    
    snprintf(url, 512, "%s%s", base_url, p);
    
    free(path);
    return url;
    
}

static int64_t file_size (const char *path) {
    struct stat sb;
    if (stat(path, &sb) < 0) return -1;
    return (int64_t)sb.st_size;
}

static bool file_delete (const char *path) {
    #ifdef WIN32
    return DeleteFileA(path);
    #else
    if (unlink(path) == 0) return true;
    #endif
    
    return false;
}

static char *file_read(const char *path, size_t *len) {
    int     fd = 0;
    off_t   fsize = 0;
    size_t  fsize2 = 0;
    char    *buffer = NULL;
    
    fsize = (off_t) file_size(path);
    if (fsize < 0) goto abort_read;
    
    int oflags = O_RDONLY;
    fd = open(path, oflags);
    if (fd < 0) goto abort_read;
    
    buffer = (char *)malloc((size_t)fsize + 1);
    if (buffer == NULL) goto abort_read;
    buffer[fsize] = 0;
    
    fsize2 = read(fd, buffer, (size_t)fsize);
    if (fsize2 != fsize) goto abort_read;
    
    if (len) *len = fsize2;
    close(fd);
    return (char *)buffer;
    
abort_read:
    if (buffer) free((void *)buffer);
    if (fd >= 0) close(fd);
    return NULL;
}

// MARK: -

static bool check_line (const char *current, const char *begin_with, const char *end_with) {
    char *found1 = NULL;
    char *found2 = NULL;
    
    found1 = strstr(current, begin_with);
    if (found1) found2 = strstr(current, end_with);
    
    return ((found1 != NULL) && (found2 != NULL));
}

static char *match_copy(const char *str, const char match) {
    const char *pos = strchr(str, match);
    
    if (pos != NULL) {
        size_t length = pos - str;
        
        char *cp_str = (char *)malloc(length + 1);
        if (cp_str == NULL) return NULL;
        
        strncpy(cp_str, str, length);
        cp_str[length] = '\0';
        
        return cp_str;
    }
    
    return NULL;
}

static char *process_md (const char *input, char *buffer, size_t *len, char *astro_header, size_t *header_len) {

    bool is_code = false;
    int toskip = NO_SKIP;
    int nskip = 1;
    int i = 0, j = 0, h = 0, slug_index = 6; // 6 is the length of the string "slug: "
    
    while (input[i]) {
        int c = NEXT;
        
        if (toskip != NO_SKIP) {
            if (c == toskip) {
                if (nskip == 1) {
                    RESET_SKIP();
                } else if ((nskip == 3) && (PEEK == toskip) && (PEEK2 == toskip)) {
                    NEXT; // -
                    NEXT; // -
                    NEXT; // \n
                    RESET_SKIP();
                }
            } else if(toskip == '-' && nskip == 3) {
                if(path_using_slug && c == '\n' && PEEK == 's' && PEEK2 == 'l' && check_line(&input[i-1], "\nslug: ", "\n")) slug_index += i;
                astro_header[h++] = c;
            }
            continue;
        }
                
        switch (c) {
            case '#': {
                if (strip_md_title == false) break;
                SET_SKIP('\n');
                continue;
            }
                
            case '!': {
                SET_SKIP('\n');
                continue;
            }
                
            case '<': {
                if (strip_html == false) break;
                SET_SKIP('>');
                continue;
            }
                
            case '{': {
                if (strip_jsx == false) break;
                SET_SKIP('}');
                continue;
            }
                
            case '[':
            case ']':
            case '*': {
                // skip character
                continue;
            }
            
            case '\n': {
                // remove double \n
                if (PEEK == '\n') continue;
                break;
            }
                
            case '"':
            case '\\': {
                if (json_mode) buffer[j++] = '\\';
                break;
            }

            case '\t': {
                // replace tabulations with spaces
                if (json_mode){
                    if(PEEK == '\t'){
                        continue;
                    } else {
                        c = ' ';
                    }
                }
                break;
            }

            case ' ': {
                // remove multiple spaces
                if (PEEK == ' ') SKIP_UNTIL(' ');
                break;
            }
                
            case 'i': {
                if (strip_jsx == false) break;
                // remove import jsx statement
                if ((PEEK == 'm') && (PEEK2 == 'p') && check_line(&input[i-1], "import ", ".astro\"")) {
                    SET_SKIP('\n');
                    continue;
                }
                break;
            }
                
            case '`': {
                // check peek and peek2
                if ((PEEK == '`') && (PEEK2 == '`')) {
                    is_code = !is_code;
                    SET_SKIP('\n');
                    continue;
                }
                break;
            }
                
#if !GENERATE_SQLITE_DATABASE
            case '\'': {
                // escape single character
                buffer[j++] = '\'';
                buffer[j++] = '\'';
                continue;
            }
#endif
                
            case '(': {
                if ((PEEK == 'h') || (PEEK == '\\')) {
                    SET_SKIP(')');
                    continue;
                }
                break;
            }
                
            case '-': {
                if ((PEEK == '-') && (PEEK2 == '-') && strip_astro_header) {
                    if (i == 1) {
                        // process meta
                        SET_SKIP_N('-', 3);
                    } else {
                        SET_SKIP('\n');
                    }
                    continue;
                }
            }
        }
        
        // copy character as-is
        buffer[j++] = c;
    }
    
    *len = j;
    buffer[j] = 0;
    *header_len = h;
    astro_header[h] = 0;
    if(slug_index == 6) return NULL; // no slug found
    return match_copy(&input[slug_index], '\n');
}

static void process_json (char *input, size_t *header_len) {

    *header_len = *header_len * 2;
    char *astro_header = (char *)malloc(*header_len);
    if (!astro_header) {
        printf("Not enough memory to allocate %zu bytes.", *header_len);
        exit(-3);
    }

    int i = 0, j = 0, quotes = 0;

    astro_header[j++] = '\n';
    astro_header[j++] = '{';
    astro_header[j++] = '\n';
    astro_header[j++] = '\\';
    astro_header[j++] = '\"';
    
    while (input[i]) {
        int c = NEXT;
                
        switch (c) {
            case ':': {
                if(PEEK == ' ' && quotes == 0){
                    astro_header[j++] = '\\';
                    astro_header[j++] = '\"';
                    astro_header[j++] = ':';
                    astro_header[j++] = ' ';
                    NEXT; //skip the space
                    astro_header[j++] = '\\';
                    astro_header[j++] = '\"';
                    continue;
                }
                break;
            }
            case '\n': {
                if(PEEK && PEEK != ' ' && PEEK != '\n' && i > 1){
                    astro_header[j++] = '\\';
                    astro_header[j++] = '\"';
                    astro_header[j++] = ',';
                    astro_header[j++] = '\n';
                    astro_header[j++] = '\\';
                    astro_header[j++] = '\"';
                }
                quotes = 0;
                continue;
            }
            case '\\': {
                astro_header[j++] = '\\';
                break;
            }
            case '\'': {
                quotes++;
                astro_header[j++] = c;
                break;
            }
            case '\"': {
                quotes++;
                continue;
            }
            case '[':
            case ']':
            case '\t': {
                // skip character
                continue;
            }
        }
        
        // copy character as-is
        astro_header[j++] = c;
    }

    astro_header[j++] = '\\';
    astro_header[j++] = '\"';
    astro_header[j++] = '\n';
    astro_header[j++] = '}';
    astro_header[j++] = '\n';
    astro_header[j] = 0;
    *header_len = j;
    
    strcpy(input, astro_header);
    free(astro_header);
}

// MARK: -

static void write_line (const char *buffer, size_t blen, int add_newline) {
    if (blen == -1) blen = strlen(buffer);
    
    size_t nwrote = fwrite(buffer, blen, 1, f);
    if (add_newline) fwrite("\n", 1, 1, f);
    
    if (nwrote != 1) {
        printf("Write fails: %s.", buffer);
        exit(-6);
    }
}

#if GENERATE_SQLITE_DATABASE
static void create_database (const char *path) {
    file_delete(path);
    
    int rc = sqlite3_open(path, &db);
    if (rc != SQLITE_OK) {
        printf("Unable to create sqlite database %s.", (db) ? sqlite3_errmsg(db) : "");
        exit(-2);
    }
    
    rc = sqlite3_exec(db, "CREATE VIRTUAL TABLE IF NOT EXISTS documentation USING fts5 (url, content);", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        printf("Unable to documentation table (%s).", sqlite3_errmsg(db));
        exit(-3);
    }
}
#endif

static void create_file (const char *path) {
    file_delete(path);
    
    f = fopen(path, "w");
    if (!f) {
        printf("Unable to create sql file :%s.", path);
        exit(-2);
    }
    
    if (create_db) {
        write_line("CREATE DATABASE documentation.sqlite IF NOT EXISTS;", -1, 1);
    }
    
    if (use_database) {
        write_line("USE DATABASE documentation.sqlite;", -1, 1);
    }
    
    if (use_transaction) {
        write_line("BEGIN TRANSACTION;", -1, 1);
    }
    
    write_line("DROP TABLE IF EXISTS documentation;", -1, 1);
    if(OPTIONS_COL){
        write_line("CREATE VIRTUAL TABLE IF NOT EXISTS documentation USING fts5 (url, content, options);", -1, 1);
    } else {
        write_line("CREATE VIRTUAL TABLE IF NOT EXISTS documentation USING fts5 (url, content);", -1, 1);
    }
}

static void create_output (const char *path) {
#if GENERATE_SQLITE_DATABASE
    create_database(path);
#else
    create_file(path);
#endif
}

static void close_output (void) {
#if GENERATE_SQLITE_DATABASE
    if (db) sqlite3_close(db);
#else
    if (use_transaction) {
        write_line("COMMIT;", -1, 1);
    }
#endif
    if (f) fclose(f);
}


#if GENERATE_SQLITE_DATABASE
static void add_database_entry(const char *url, char *buffer, size_t size) {
    sqlite3_stmt *vm = NULL;
    
    int rc = sqlite3_prepare_v2(db, "INSERT INTO documentation (url, content) VALUES (?1, ?2);", -1, &vm, NULL);
    if (rc != SQLITE_OK) goto abort_add;
    
    rc = sqlite3_bind_text(vm, 1, url, -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) goto abort_add;
    
    rc = sqlite3_bind_text(vm, 2, buffer, (int)size, SQLITE_STATIC);
    if (rc != SQLITE_OK) goto abort_add;
    
    rc = sqlite3_step(vm);
    if (rc != SQLITE_DONE) goto abort_add;
    
    sqlite3_finalize(vm);
    
    return;
    
abort_add:
    printf("add_database error: %s\n", sqlite3_errmsg(db));
    exit(-10);
}
#endif

static void add_file_entry(const char *url, char *buffer, size_t bsize, char *astro_header, size_t header_size) {
    if (bsize == -1) bsize = strlen(buffer);
    if (header_size == -1) header_size = strlen(astro_header);
    
    size_t url_size = strlen(url);
    
    size_t blen;
    if(OPTIONS_COL){
        blen = url_size + bsize + header_size + 1024;
    } else {
        blen = url_size + bsize + 1024;
    }
    char *b = malloc (blen);
    if (!b) {
        exit(-11);
    }
    
    size_t nwrote;
    if(OPTIONS_COL){
        nwrote = snprintf(b, blen, "INSERT INTO documentation (url, content, options) VALUES ('%s', '%s', json('%s'));", url, buffer, astro_header);
    } else {
        nwrote = snprintf(b, blen, "INSERT INTO documentation (url, content) VALUES ('%s', '%s');", url, buffer);
    }
    write_line(b, nwrote, 1);
    
    free(b);
}

static void add_entry(const char *url, char *buffer, size_t size, char *astro_header, size_t header_size) {
#if GENERATE_SQLITE_DATABASE
    add_database_entry(url, buffer, size);
#else
    add_file_entry(url, buffer, size, astro_header, header_size);
#endif
}

static void scan_docs (const char *base_url, const char *dir_path) {
    DIRREF dir = opendir(dir_path);
    if (!dir) return;
    
    const char *target_file;
    while ((target_file = directory_read(dir))) {
        
        // if file is a folder then start recursion
        const char *full_path = file_buildpath(target_file, dir_path);
        if (is_directory(full_path)) {
            scan_docs(base_url, full_path);
            continue;
        }
        
        // test only files with a .md or mdx extension
        if ((strstr(full_path, ".md") == NULL) && (strstr(full_path, ".mdx") == NULL)) continue;
        
        // load md source code
        size_t size = 0;
        char *source_code = file_read(full_path, &size);
        
        size_t header_size = size;
        char *buffer = (char *)malloc(size);
        char *astro_header = (char *)malloc(header_size);
        if (!buffer || !astro_header) {
            printf("Not enough memory to allocate %zu bytes.", size > header_size ? size : header_size);
            exit(-3);
        }

        const char *slug_path = process_md(source_code, buffer, &size, astro_header, &header_size);

        if(OPTIONS_COL) process_json(astro_header, &header_size);

        // build url and title
        char *url;
        if(path_using_slug && slug_path != NULL){
            url = malloc(strlen(base_url) + strlen(slug_path) + 1);
            strcpy(url, base_url);
            strcat(url, slug_path);
        } else {
            url = file_buildurl(base_url, full_path);
        }
        
        add_entry(url, buffer, size, astro_header, header_size);
        
        //DEBUG
        //printf("title: %s\n", title);
        //printf("url:   %s\n", url);
        //printf("%s\n", src_path);
        //printf("%s\n", full_path);
        //printf("INPUT:\n%s\n\n", source_code);
        //printf("OUTPUT:\n%s\n", buffer);
        
        free((void *)url);
        free((void *)full_path);
        free((void *)slug_path);
        free(source_code);
        free(buffer);
        free(astro_header);
    }
}

// MARK: -

int main (int argc, char * argv[]) {
    // setup arguments
    static struct cag_option options[] = {
        {
            .identifier = 'i',
            .access_letters = "i",
            .access_name = "input",
            .value_name = "input_docs_path",
            .description = "Input documentation path"
        },
        
        {
            .identifier = 'o',
            .access_letters = "o",
            .access_name = "output",
            .value_name = "output_path",
            .description = "Output path"
        },

        {
            .identifier = 'b',
            .access_letters = "b",
            .access_name = "base-url",
            .value_name = "base_url",
            .description = "Base url in docs path"
        },
        
        {
            .identifier = 'a',
            .access_letters = "a",
            .access_name = "strip-astro-header",
            .value_name = NULL,
            .description = "Remove Astro header"
        },
        
        {
            .identifier = 'm',
            .access_letters = "m",
            .access_name = "strip-md-titles",
            .value_name = NULL,
            .description = "Strip Markdown titles"
        },
        
        {
            .identifier = 'l',
            .access_letters = "l",
            .access_name = "strip-html",
            .value_name = NULL,
            .description = "Strip HTML tags"
        },
        
        {
            .identifier = 'j',
            .access_letters = "j",
            .access_name = "strip-jsx",
            .value_name = NULL,
            .description = "Strip JSX tags"
        },
        
        {
            .identifier = 'c',
            .access_letters = "c",
            .access_name = "create-database",
            .value_name = NULL,
            .description = "Add a CREATE DATABASE statement"
        },
        
        {
            .identifier = 'u',
            .access_letters = "u",
            .access_name = "use-database",
            .value_name = NULL,
            .description = "Add a USE DATABASE statement"
        },
        
        {
            .identifier = 't',
            .access_letters = "t",
            .access_name = "use-transactions",
            .value_name = NULL,
            .description = "Use transactions"
        },

        {
            .identifier = 'g',
            .access_letters = "g",
            .access_name = "path-using-slug",
            .value_name = NULL,
            .description = "Use the slug in the header as the path instead of the relative one"
        },
        
        {
            .identifier = 's',
            .access_letters = "s",
            .access_name = "json",
            .value_name = NULL,
            .description = "JSON mode"
        },
        
        {
            .identifier = 'h',
            .access_letters = "h",
            .access_name = "help",
            .value_name = NULL,
            .description = "Shows the command help"
        },
        
    };
    
    cag_option_context context;
    cag_option_init(&context, options, CAG_ARRAY_SIZE(options), argc, argv);
    
    while (cag_option_fetch(&context)) {
        switch (cag_option_get_identifier(&context)) {
            case 'i': src_path = cag_option_get_value(&context); break;
            case 'o': dest_path = cag_option_get_value(&context); break;
            case 'b': base_url = cag_option_get_value(&context); break;
            case 'l': strip_html = true; break;
            case 'j': strip_jsx = true; break;
            case 'm': strip_md_title = true; break;
            case 'a': strip_astro_header = true; break;
            case 't': use_transaction = true; break;
            case 'u': use_database = true; break;
            case 's': json_mode = true; break;
            case 'g': path_using_slug = true; break;
            case 'c': create_db = true; break;
                
            case 'h':
                printf("Usage: docbuilder [OPTION]...\n");
                printf("A tool to generate SQLite FTS-5 statements from md and mdx files.\n\n");
                cag_option_print(options, CAG_ARRAY_SIZE(options), stdout);
                return EXIT_SUCCESS;
        
            case '?':
                cag_option_print_error(&context, stdout);
                break;
        }
      }
    
    create_output(dest_path);
    scan_docs(base_url, src_path);
    close_output();
    
    return 0;
}
