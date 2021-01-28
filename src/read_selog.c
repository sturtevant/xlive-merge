#include <cwalk.h>
#include <xlive-merge.h>

uint32_t swap_uint32( uint32_t val )
{
    val = ((val << 8) & 0xFF00FF00 ) | ((val >> 8) & 0xFF00FF ); 
    return (val << 16) | (val >> 16);
}

selog *read_selog(const char *folder)
{
    const char *selog_filename = "SE_LOG.BIN";
    const int selog_path_len = strlen(folder) + strlen(selog_filename) + 2;
    char path_buffer[selog_path_len];
    cwk_path_join(folder, selog_filename, path_buffer, selog_path_len);

    FILE *f = fopen(path_buffer, "rb");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *data = malloc(fsize + 1);
    fread(data, 1, fsize, f);
    fclose(f);

    selog *result = malloc(sizeof(selog));

    //memcpy(result->session_name, data, 4);
    result->session_name = *((uint32_t*)data);
    result->channels = (*((uint32_t*)(data+4)));
    result->sample_rate = (*((uint32_t*)(data+8)));
    result->session_name_2 = *((uint32_t*)(data+12));
    result->take_count = *((uint32_t*)(data+16));
    result->marker_count = *((uint32_t*)(data+20));
    result->total_length = *((uint32_t*)(data+24));
    memcpy(result->user_name, data+1552, 17);

    return result;
}


//
//	SE_LOG.BIN is a 2kB (2048) file made of:
// 		   1:	<session name> = session name built from yymmhhmm in hex form (on 32 bits)
//		   5:	<nb of channels> = 32bit int
//		   9:	<sample rate> = 32bits unsigned int
// 		  13:	<session name> = session name built from yymmhhmm in hex form (on 32 bits)
//		  17:	<nb of takes> = number of xxx.wav files associated to session on 32bit int
//		  21:	<nb markers> = number of markers on 32bit int
//		  25:	<total length> = length on unsigned int
//		  29:	<take size> = size of take[i] on int i:[0...256[
//		...		<filler = 0> = fill with 0 until address 1051.
//		1052:	<markers> = marker position in second * sample rate on 32bit unsigned int
//		...		<filler = 0> = fill with 0 until address 1151
//		1553:	<name (user)> 16 char max name
//		...		<filler = 0> = fill with 0 until address 2047, inclusive
//