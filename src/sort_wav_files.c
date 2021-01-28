#include <xlive-merge.h>

typedef struct
{
    selog *selog;
    file_item *item;
} sortable_file_item;

typedef struct
{
    char *path;
    selog *selog;
} selog_map_entry;

selog *get_selog(char *path, selog_map_entry *map, int *count)
{
    int m;
    for (m = 0; m < *count; m++)
    {
        if (strcmp(map[m].path, path) == 0)
            return map[m].selog;
    }
    selog *s = read_selog(path);
    map[*count].path = path;
    map[*count].selog = s;
    (*count)++;
    return s;
}

int cmpfunc (const void * a_ptr, const void * b_ptr) {
    const sortable_file_item *a = a_ptr;
    const sortable_file_item *b = b_ptr;

    if (a->selog != NULL && b->selog != NULL)
    {
        int session_name_cmp = a->selog->session_name - b->selog->session_name;
        if (session_name_cmp != 0)
            return session_name_cmp;
    }
    if (a->selog != NULL && b->selog != NULL)
    {
        int user_name_cmp = strcmp(a->selog->user_name, b->selog->user_name);
        if (user_name_cmp != 0)
            return user_name_cmp;
    }
    return strcmp(a->item->name, b->item->name);
}

void sort_wav_files(file_list *files, char use_selog)
{
    sortable_file_item *list = malloc(sizeof(sortable_file_item)*files->count);

    selog_map_entry *map = malloc(sizeof(selog_map_entry)*files->count);
    int map_count = 0;

    int i;
    uint32_t session_name = 0;
    for (i = 0; i < files->count; i++)
    {
        list[i].item = files->items[i];
        if (use_selog)
        {
            list[i].selog = get_selog(files->items[i]->path, map, &map_count);
            if (session_name == 0)
                session_name = list[i].selog->session_name;
            else if (session_name != list[i].selog->session_name)
                fprintf(stderr, "Warning: Mismatched session identifiers. These files may not all be from the same recording set!\n");
        }
        else
            list[i].selog = NULL;
    }

    qsort(list, files->count, sizeof(sortable_file_item), cmpfunc);

    for (i = 0; i < files->count; i++)
    {
        files->items[i] = list[i].item;
    }

    free(map);
    free(list);
}