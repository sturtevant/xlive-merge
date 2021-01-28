#define DR_WAV_IMPLEMENTATION
#include <cwalk.h>
#include <xlive-merge.h>
#include <math.h>

#define BUFFER_FRAMES 2880000 // 1 min

#define MAX_OUTPUT_FILENAME_LENGTH 20 // e.g. channel-###.wav\0

#define PBSTR "############################################################"
#define PBWIDTH 60

typedef struct
{
    int32_t max;
    double dB;
} channel_stats;

void printProgress(double percentage) {
    int val = (int) (percentage * 100);
    int lpad = (int) (percentage * PBWIDTH);
    int rpad = PBWIDTH - lpad;
    printf("\r%3d%% [%.*s%*s]", val, lpad, PBSTR, rpad, "");
    fflush(stdout);
}

drwav_bool8 are_merge_compatible(drwav *inputs, size_t count)
{
    drwav_container container;
    drwav_uint32 sampleRate;
    drwav_uint16 channels;
    drwav_uint16 bitsPerSample;

    int i;
    for (i = 0; i < count; i++)
    {
        if (i == 0)
        {
            container = inputs[i].container;
            sampleRate = inputs[i].sampleRate;
            channels = inputs[i].channels;
            bitsPerSample = inputs[i].bitsPerSample;
        }
        else
        {
            if (container != inputs[i].container) return DRWAV_FALSE;
            if (sampleRate != inputs[i].sampleRate) return DRWAV_FALSE;
            if (channels != inputs[i].channels) return DRWAV_FALSE;
            if (bitsPerSample != inputs[i].bitsPerSample) return DRWAV_FALSE;
        }
    }

    return DRWAV_TRUE;
}

void process_frames_s32(const int32_t *inbuf, int32_t *outbuf[], const int frames, const drwav_uint16 channels, channel_stats *stats)
{
    int f;
    drwav_uint16 c;
    for (f = 0; f < frames; f++)
    {
        for (c = 0; c < channels; c++)
        {
            int32_t x = inbuf[f*channels+c];

            outbuf[c][f] = x;

            int32_t abs = (x < 0) ? -x: x;
            if (abs > stats[c].max)
                stats[c].max = abs;
        }
    }
}

void process_frames_s16(const int16_t *inbuf, int16_t *outbuf[], const int frames, const drwav_uint16 channels, channel_stats *stats)
{
    int f;
    drwav_uint16 c;
    for (f = 0; f < frames; f++)
    {
        for (c = 0; c < channels; c++)
        {
            int16_t x = inbuf[f*channels+c];

            outbuf[c][f] = x;

            int32_t abs = (x < 0) ? -x: x;
            if (abs > stats[c].max)
                stats[c].max = abs;
        }
    }
}

int split_merge(const char *input_files[], const size_t input_count, const char *output_folder)
{
    drwav inputs[input_count];

    if (input_count < 1)
    {
        printf("Error: Must provide at least one input file.\n");
        return -1;
    }

    int i;
    uint64_t totalFrames = 0;
    for (i = 0; i < input_count; ++i)
    {
        if (!drwav_init_file(&inputs[i], input_files[i], NULL))
        {
            printf("Error loading input file: %s\n", input_files[i]);
            return -1;
        }
        totalFrames += inputs[i].totalPCMFrameCount;
    }

    if (!are_merge_compatible(inputs, input_count))
    {
        printf("Cannot merge - mismatched formats.\n");
        return -1;
    }

    const drwav_uint32 sampleRate = inputs[0].sampleRate;
    const drwav_uint16 bitsPerSample = inputs[0].bitsPerSample;
    const size_t bytesPerSample = bitsPerSample / 8;
    const drwav_uint16 channels = inputs[0].channels;

    switch (bitsPerSample)
    {
        case 32:
        case 16:
            break;
        default:
            printf("Unsupported bit rate (%i bit)\n", bitsPerSample);
            return -1;
    }

    const size_t outbuf_size = BUFFER_FRAMES * bytesPerSample;
    const size_t inbuf_size = outbuf_size * channels;

    void *buffers = malloc(2 * inbuf_size);
    void *inbuf = buffers;
    void *outbuf[channels];
    drwav outputs[channels];
    channel_stats stats[channels];

    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_PCM;
    format.channels = 1;
    format.sampleRate = sampleRate;
    format.bitsPerSample = bitsPerSample;

    short c;
    for (c = 0; c < channels; c++)
    {
        outbuf[c] = malloc(BUFFER_FRAMES * bytesPerSample);

        char output_filename[MAX_OUTPUT_FILENAME_LENGTH];
        snprintf(output_filename, sizeof(output_filename), "channel-%i.wav", c+1);

        const int output_file_length = strlen(output_folder) + strlen(output_filename) + 2;
        char output_file[output_file_length];
        cwk_path_join(output_folder, output_filename, output_file, sizeof(output_file));
        drwav_init_file_write(&outputs[c], output_file, &format, NULL);

        stats[c].max = 0;
    }

    const int log_file_path_length = strlen(output_folder) + 16 + 2;
    char log_file_path[log_file_path_length];
    cwk_path_join(output_folder, "channel-info.txt", log_file_path, sizeof(log_file_path));
    FILE *log_file = fopen(log_file_path, "w");
 
    if (log_file == NULL) {
        fprintf(stderr, "File (%s) can't be opened.\n", log_file_path);
        return -1;
    }

    uint64_t totalFramesProcessed = 0;
    for (i = 0; i < input_count; i++)
    {
        drwav_uint64 framesRead;
        do
        {
            switch (bitsPerSample)
            {
                case 32:
                    framesRead = drwav_read_pcm_frames_s32(&inputs[i], BUFFER_FRAMES, inbuf);
                    process_frames_s32((int32_t*)inbuf, (int32_t**)outbuf, framesRead, channels, stats);
                    break;
                case 16:
                    framesRead = drwav_read_pcm_frames_s16(&inputs[i], BUFFER_FRAMES, inbuf);
                    process_frames_s16((int16_t*)inbuf, (int16_t**)outbuf, framesRead, channels, stats);
                    break;
            }

            for (c = 0; c < channels; c++)
            {
                drwav_uint64 framesWritten = drwav_write_pcm_frames(&outputs[c], framesRead, outbuf[c]);
                if (framesWritten != framesRead)
                {
                    printf("Wrote fewer frames to output than read from input! (read: %llu, wrote: %llu)\n", framesRead, framesWritten);
                    return -1;
                }
            }

            totalFramesProcessed += framesRead;
            printProgress((double)totalFramesProcessed / totalFrames);
        }
        while (framesRead == BUFFER_FRAMES);
    }

    double ref_dB;
    switch (bitsPerSample)
    {
        case 32:
            ref_dB = log10(pow(2147483647, 20));
            break;
        case 16:
            ref_dB = log10(pow(32767, 20));
            break;
    }

    fprintf(stdout, "\n");
    for (c = 0; c < channels; c++)
    {
        drwav_uninit(&outputs[c]);
        
        stats[c].dB =  log10(pow(stats[c].max, 20)) - ref_dB;

        fprintf(stdout, "Channel %i | %3.2f dB\n", c+1, stats[c].dB);
        fprintf(log_file, "Channel %i | %3.2f dB\n", c+1, stats[c].dB);
    }

    fprintf(log_file, "************************************************\n");
    fprintf(log_file, "# Input Files: %zu\n", input_count);
    fprintf(log_file, "Input Files:\n");
    for (i = 0; i < input_count; i++)
        fprintf(log_file, " - %s\n", input_files[i]);
    fprintf(log_file, "Sample Rate: %i\n", sampleRate);
    fprintf(log_file, "Bit Rate: %i\n", bitsPerSample);
    fprintf(log_file, "Channels: %i\n", channels);

    fclose(log_file);
    free(buffers);

    return 0;
}
