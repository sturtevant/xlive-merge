#include <dr_wav.h>

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define DIRENT_DIRECTORY 4 // d_type for folders
#define DIRENT_FILE 8 // d_type for files

// represents the rules for which paths should be evaluated at a given level of the tree
typedef struct
{
    char type_filter; // NULL or a dirent type
    char *pattern_filter; // NULL or a regex string
} file_search_rule;

typedef struct
{
    char *name;
    char *path;
    char type;
} file_item;

typedef struct
{
    file_item **items;
    int count;
} file_list;

file_list *find_files(const char *starting_paths[], const int count, const file_search_rule rules[], const int levels);
file_list *verify_files(const char *files[], const int count);
void free_list(file_list *list);

void sort_wav_files(file_list *files, char use_selog);

typedef struct
{
    uint32_t session_name;
    uint32_t channels;
    uint32_t sample_rate;
    uint32_t session_name_2;
    uint32_t take_count;
    uint32_t marker_count;
    uint32_t total_length;
    uint32_t take_sizes[255];
    uint32_t markers[125];
    char user_name[17];
} selog;

selog *read_selog(const char *folder);

int split_merge(const char *input_files[], const size_t input_count, const char *output_folder);
