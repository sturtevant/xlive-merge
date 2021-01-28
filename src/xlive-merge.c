#include <cwalk.h>
#include <unistd.h>
#include <xlive-merge.h>

#define DEFAULT_INPUT_PATH "/Volumes"

int main(int argc, char **argv)
{
    int file_flag = 0;
    int quiet_flag = 0;
    int output_flag = 0;
    char *output_path = NULL;
    char **input_paths;
    int input_paths_count;

    int i;
    while ((i = getopt(argc, argv, "fo:")) != -1)
        switch (i)
        {
        case 'f':
            file_flag = 1;
            break;
        case 'o':
            output_flag = 1;
            output_path = optarg;
            break;
        case '?':
            if (optopt == 'o')
                fprintf(stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint(optopt))
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
            return 1;
        default:
            abort();
        }

    if (optind < argc)
    {
        input_paths_count = argc-optind;
        input_paths = malloc(sizeof(char*)*input_paths_count);
        int j = 0;
        for (i = optind; i < argc; i++) {
            input_paths[j++] = argv[i];
        }
    }
    else
    {
        input_paths_count = 1;
        input_paths = malloc(sizeof(char*)*input_paths_count);
        input_paths[0] = DEFAULT_INPUT_PATH;
    }

    if (output_flag == 0)
    {
        printf("usage: $ %s -o <output-path>  [-f] [ <input-path> ... <input-path> ]\n", argv[0]);
        printf("  -o  the destination of the output files\n");
        printf("  -f  specify input files instead of reading from the SD Card(s)\n");
        printf("  (default SD Card mount/search path is /Volumes)\n");
        printf("\n");
        return -1;
    }

    file_list *wav_files;
    if (file_flag)
    {
        wav_files = verify_files((const char **)input_paths, input_paths_count);
        sort_wav_files(wav_files, 0);
    }
    else
    {
        file_search_rule rules[4];
        rules[0].type_filter = DIRENT_DIRECTORY;
        rules[0].pattern_filter = NULL;
        rules[1].type_filter = DIRENT_DIRECTORY;
        rules[1].pattern_filter = "X_LIVE";
        rules[2].type_filter = DIRENT_DIRECTORY;
        rules[2].pattern_filter = NULL;
        rules[3].type_filter = DIRENT_FILE;
        rules[3].pattern_filter = "^[0-9]\\{1,\\}\\.wav$";

        wav_files = find_files((const char **)input_paths, input_paths_count, rules, 4);
        sort_wav_files(wav_files, 1);
    }

    const char **input_files = malloc(sizeof(char*) * wav_files->count);
    const int count = wav_files->count;
    for (i = 0; i < count; i++)
    {
        int length = strlen(wav_files->items[i]->path) + strlen(wav_files->items[i]->name) + 2;
        char *name = malloc(sizeof(char) * length);
        cwk_path_join(wav_files->items[i]->path, wav_files->items[i]->name, name, length);
        input_files[i] = name;
    }
    free_list(wav_files);

    split_merge(input_files, count, output_path);
}
