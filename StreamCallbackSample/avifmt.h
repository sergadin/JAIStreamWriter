#ifndef _AVIFMT_H_
#define _AVIFMT_H_

#define SWAP2(x) (((x>>8) & 0x00ff) | ((x<<8) & 0xff00)) 
#define SWAP4(x) (((x>>24) & 0x000000ff) | ((x>>8)  & 0x0000ff00) | ((x<<8)  & 0x00ff0000) | ((x<<24) & 0xff000000)) 
#define LILEND2(a) (a) 
#define LILEND4(a) (a) 
#define BIGEND2(a) SWAP2((a)) 
#define BIGEND4(a) SWAP4((a)) 

//typedef int WORD;
//typedef unsigned int DWORD;
//typedef char BYTE;

const DWORD AVIF_HASINDEX = 0x00000010;
const DWORD AVIF_MUSTUSEINDEX = 0x00000020;
const DWORD AVIF_ISINTERLEAVED = 0x00000100;
const DWORD AVIF_TRUSTCKTYPE = 0x00000800;
const DWORD AVIF_WASCAPTUREFILE = 0x00010000;
const DWORD AVIF_COPYRIGHTED = 0x00020000;

struct AVI_avih
{
	DWORD	us_per_frame;
	DWORD max_bytes_per_sec;
	DWORD padding;
	DWORD flags;
	DWORD tot_frames;
	DWORD init_frames;
	DWORD streams;
	DWORD buff_sz;
	DWORD width;
	DWORD height;
	DWORD reserved[4];
};

struct rcFrame
{
	short int left;
	short int top;
	short int right;
	short int bottom;
};

struct AVI_strh
{
	unsigned char type[4];
	unsigned char handler[4];
	DWORD flags;
	DWORD priority;
	DWORD init_frames;
	DWORD scale;
	DWORD rate;
	DWORD start;
	DWORD length;
	DWORD buff_sz;
	DWORD quality;
	DWORD sample_sz;
	struct rcFrame rc;
};


struct AVI_strf
{
	DWORD sz;
	DWORD width;
	DWORD height;
	DWORD planes_bit_cnt;
	unsigned char compression[4];
	DWORD image_sz;
	DWORD xpels_meter;
	DWORD ypels_meter;
	DWORD num_colors;
	DWORD imp_colors;
};

struct AVI_list_hdr
{
	unsigned char id[4];
	DWORD sz;
	unsigned char type[4];
};

struct AVI_list_odml
{
	struct AVI_list_hdr list_hdr;
	unsigned char id[4];
	DWORD sz;
	DWORD frames;
};

struct AVI_list_strl
{
	struct AVI_list_hdr list_hdr;
	unsigned char strh_id[4];
	DWORD strh_sz;
	struct AVI_strh strh;
	unsigned char strf_id[4];
	DWORD strf_sz;
	struct AVI_strf strf;
	struct AVI_list_odml list_odml;
};

struct AVI_list_hdrl
{
	struct AVI_list_hdr list_hdr;
	unsigned char avih_id[4];
	DWORD avih_sz;
	struct AVI_avih avih;
	struct AVI_list_strl strl;
};


struct AVI_file_header
{
	unsigned char const_riff[4];
	DWORD riff_size;
	unsigned char const_avi[4];
	struct AVI_list_hdrl hdrl;
	unsigned char const_list[4];
	DWORD list_size;
	unsigned char const_movi[4];
};

#endif
