#include <dirent.h>
#include <regex.h>
#include <cwalk.h>
#include <xlive-merge.h>

#define MAX_INPUT_FILEPATH_LENGTH 4095

typedef struct file_linked_list_struct
{
    struct file_linked_list_struct *prev;
    struct file_linked_list_struct *next;
    char *name;
    char *path;
    char type;
} file_linked_list;

bool is_match(const char *text, const char* pattern)
{
    regex_t regex;
    int reti;
    char msgbuf[100];

    /* Compile regular expression */
    reti = regcomp(&regex, pattern, REG_ICASE);
    if (reti) {
        fprintf(stderr, "Could not compile regex\n");
        return false;
    }

    /* Execute regular expression */
    reti = regexec(&regex, text, 0, NULL, 0);
    if (!reti) {
        return true;
    }
    else if (reti == REG_NOMATCH) {
        return false;
    }
    else {
        regerror(reti, &regex, msgbuf, sizeof(msgbuf));
        fprintf(stderr, "Regex match failed: %s\n", msgbuf);
        return false;
    }

    /* Free memory allocated to the pattern buffer by regcomp() */
    regfree(&regex);
}

void add_entry(file_linked_list **first, const char *name, const char *path, const char type)
{
    file_linked_list *to_add = malloc(sizeof(file_linked_list));
    to_add->name = strcpy(malloc(sizeof(char) * (strlen(name) + 1)), name);
    to_add->path = strcpy(malloc(sizeof(char) * (strlen(path) + 1)), path);
    to_add->type = type;

    if (*first == NULL)
    {
        to_add->prev = NULL;
        to_add->next = NULL;
        *first = to_add;
        return;
    }
    
    file_linked_list *curr = *first;
    while (curr->next != NULL)
    {
        curr = curr->next;
    }

    curr->next = to_add;
    to_add->prev = curr;
    to_add->next = NULL;
}

file_list *create_list(file_linked_list *first)
{
    int count = 0;
    file_linked_list *curr = first;
    while (curr != NULL)
    {
        curr = curr->next;
        count++;
    }

    file_list *result = malloc(sizeof(file_list));
    result->count = count;
    
    if (count == 0)
    {
        result->items = NULL;
        return result;
    }

    result->items = malloc(sizeof(file_item*) * count);
    curr = first;
    int i = 0;
    while (curr != NULL)
    {
        result->items[i] = malloc(sizeof(file_item));
        result->items[i]->name = curr->name;
        result->items[i]->path = curr->path;
        result->items[i]->type = curr->type;

        file_linked_list *to_free = curr;
        curr = curr->next;
        free(to_free);
        i++;
    }
    return result;
}

void free_list(file_list *list)
{
    int i;
    for (i = 0; i < list->count; i++)
    {
        free(list->items[i]->name);
        free(list->items[i]->path);
        free(list->items[i]);
    }
    free(list->items);
    free(list);
}

void enumerate_files(file_linked_list **first, const char *path, const file_search_rule rule)
{
    DIR *dir = opendir(path);
    if (!dir)
        return;
    struct dirent *en;
    while ((en = readdir(dir)) != NULL) {
        if (en->d_name[0] == '.')
            continue;
        if (rule.type_filter != 0 && en->d_type != rule.type_filter)
        {
            //printf("rejected: type-mismatch `%s`\n", en->d_name);
            continue;
        }
        if (rule.pattern_filter != NULL && !is_match(en->d_name, rule.pattern_filter))
        {
            //printf("rejected: pattern-mismatch `%s` (%s)\n", en->d_name, rule.pattern_filter);
            continue;
        }
        add_entry(first, en->d_name, path, en->d_type);
        //printf("added: `%s`\n", en->d_name);
    }
    closedir(dir);
}

// enumerates the files (matching a series of given per-level rules) in one or more trees
file_list *find_files(const char *starting_paths[], const int count, const file_search_rule rules[], const int levels)
{
    if (starting_paths == 0 || levels == 0)
        return NULL;

    file_list *prev;
    file_linked_list *linked_list = NULL;
    int l, p;

    for (p = 0; p < count; p++)
        enumerate_files(&linked_list, starting_paths[p], rules[0]);
    prev = create_list(linked_list);

    for (l = 1; l < levels; l++)
    {
        linked_list = NULL;
        for (p = 0; p < prev->count; p++)
        {
            char path_buffer[MAX_INPUT_FILEPATH_LENGTH];
            cwk_path_join(prev->items[p]->path, prev->items[p]->name, path_buffer, MAX_INPUT_FILEPATH_LENGTH);
            enumerate_files(&linked_list, path_buffer, rules[l]);
        }
        free_list(prev);
        prev = create_list(linked_list);
    }
    return prev;
}

// enumerates the files and makes sure they can be accessed
file_list *verify_files(const char *files[], const int count)
{
    if (count == 0)
        return NULL;

    file_list *list;
    file_linked_list *linked_list = NULL;
    int i;

    for (i = 0; i < count; i++)
    {
        if (access(files[i], F_OK) == 0) {
            add_entry(&linked_list, files[i], "", DIRENT_FILE);
        }
    }
    return create_list(linked_list);
}
