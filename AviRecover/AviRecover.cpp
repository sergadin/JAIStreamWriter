// AviRecover.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <list>

#include <MMC.h>
#include "../StreamCallbackSample/avifmt.h"

#include "../StreamCallbackSample/avi_writer.h"

struct StreamStats{
	size_t framesCount;
	off64_t jpg_sz_64;
	off64_t offset;
	std::list<FrameData> framesList;
	
	StreamStats() {
		framesCount = 0;
		jpg_sz_64 = 0;
		offset = 4;
	}

	void next_frame(size_t size) {
		FrameData f(size, offset);
		offset += size + 8;
		jpg_sz_64 += size;
		framesCount++;
		framesList.push_back(f);
	}

	void print() {
		printf("frames: %d\n", framesCount);
	}
} streamData;


void usage(_TCHAR *name) {
	std::wcerr << "Usage: " << name << " <file.avi>" << std::endl;
}


void print_quartet(unsigned int i, FILE *out)
{
	putc(i % 0x100, out);
	i /= 0x100;
	putc(i % 0x100, out);
	i /= 0x100;
	putc(i % 0x100, out);
	i /= 0x100;
	putc(i % 0x100, out);
}


int read_quartet(unsigned int *i, FILE *in)
{
	unsigned char buf[4];
	if (4 != fread(buf, 1, 4, in)) {
		printf("Unable to read quartet.\n");
		return 0;
	}

	*i = 0;
	*i = (buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24));

	return 1;
}

bool finalize(FILE *out, AVI_file_header *hHdr);
bool updateHeader(AVI_file_header *pHdr, FILE *out);

struct Frame {
	DWORD size;
	unsigned char *data;
	Frame(size_t sz) {
		size = sz;
		data = new unsigned char[sz];
	}

	~Frame() {
		delete[] data;
	}
};

Frame* read_frame(FILE *f) {
	unsigned char buf[4];
	unsigned int frame_size = 0;
	long position = ftell(f);

	if (4 != fread(buf, 1, 4, f)) { // reading 00dc
		printf("Unable to read frame 00dc prefix.\n");
		goto failure;
	}

	if (!read_quartet(&frame_size, f)) {
		printf("Unable to read frame size.\n");
		goto failure;
	}

	Frame *frame = new Frame(frame_size);
	size_t bytes_read = fread(frame->data, 1, frame_size, f);
	if (frame_size != bytes_read) {
		printf("Unexpected end of frame: %d bytes expected, %d read\n", frame_size, bytes_read);
		delete frame;
		goto failure;
	}
	return frame;
failure:
	fseek(f, position, SEEK_SET);
	return NULL;
}

int _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 2) {
		usage(argv[0]);
		exit(1);
	}
	_TCHAR *fname = argv[1];

	FILE *avi_stream;
	_wfopen_s(&avi_stream, fname, _T("r+b"));

	struct AVI_file_header hdr;
	size_t header_size = fread_s(&hdr, sizeof(hdr), 1, sizeof(hdr), avi_stream);
	if (hdr.hdrl.avih.width > 1 || hdr.hdrl.avih.height > 1) {
		printf("File was recovered?");
		exit(0);
	}

	Frame *pFrame;
	while ((pFrame = read_frame(avi_stream)) != NULL)
	{
		streamData.next_frame(pFrame->size);

		if (streamData.framesCount > 12300) {
			FILE *jpg;
			fopen_s(&jpg, "frame.jpg", "wb");
			fwrite(pFrame->data, 1, pFrame->size, jpg);
			fclose(jpg);
		}
		delete pFrame;
	}
	long position = ftell(avi_stream);
	_chsize_s(_fileno(avi_stream), position);

	finalize(avi_stream, &hdr);
	fclose(avi_stream);

	printf("%d frames found.\n", streamData.framesCount);
	return 0;
}



/* *********************** */


bool finalize(FILE *out, AVI_file_header *pHdr)
{
	printf("Finalizing...\n");
	putc('i', out);
	putc('d', out);
	putc('x', out);
	putc('1', out);
	size_t framesCount = streamData.framesCount;
	print_quartet(16 * framesCount, out);
	for (std::list<FrameData>::const_iterator f = streamData.framesList.begin(); f != streamData.framesList.end(); f++)
	{
		putc('0', out);
		putc('0', out);
		putc('d', out);
		putc('c', out);
		print_quartet(18, out);
		print_quartet(f->offset, out);
		print_quartet(f->size, out);
	}
	updateHeader(pHdr, out); // Update framecount and other header's fields
	return true;
}


bool updateHeader(AVI_file_header *pHdr, FILE *out)
{
	DWORD per_usec = 1;
	long jpg_sz = 1;
	const off64_t MAX_RIFF_SZ = 2147483648L; // 2 GB limit   
	DWORD riff_sz;
	DWORD frames = streamData.framesCount;
	DWORD rate = 25;
	DWORD us_per_frame = 1000000/rate;

	int width = 1280;
	int height = 720;
	
	off64_t jpg_sz_64 = streamData.jpg_sz_64;
	if (jpg_sz_64 == 0)
		jpg_sz_64 = 1024 * 1024; // <==================== total file size; get_file_sz(frlst);

	off64_t riff_sz_64 = sizeof(struct AVI_list_hdrl) + 4 + 4 + streamData.jpg_sz_64 + 8 * streamData.framesCount + 8 + 8 + 16 * streamData.framesCount;

	if (riff_sz_64 >= MAX_RIFF_SZ)
	{
		fprintf(stderr, "RIFF would exceed 2 Gb limit\n");
		riff_sz = 0;
		//return false;
	}

	jpg_sz = (long)streamData.jpg_sz_64;
	riff_sz = (DWORD)riff_sz_64;

	//printf("jpeg_sz=%ul, riff_sz=%ul\n", jpg_sz, riff_sz);
	riff_sz = 0;   // <==================================== FIXME: calculate riff size correctly 

	pHdr->riff_size = LILEND4(riff_sz);
	pHdr->list_size = LILEND4(jpg_sz + 8 * frames + 4);
	pHdr->hdrl.avih.tot_frames = LILEND4(frames);
	pHdr->hdrl.avih.us_per_frame = LILEND4(us_per_frame);
	pHdr->hdrl.avih.max_bytes_per_sec = LILEND4( (jpg_sz / frames) / rate);

	pHdr->hdrl.strl.strh.length = LILEND4(frames);
	pHdr->hdrl.strl.list_odml.frames = LILEND4(frames);

	pHdr->hdrl.avih.width = LILEND4(width);
	pHdr->hdrl.avih.height = LILEND4(height);
	pHdr->hdrl.strl.strf.width = LILEND4(width);
	pHdr->hdrl.strl.strf.height = LILEND4(height);
	pHdr->hdrl.strl.strf.image_sz = LILEND4(width * height * 3);

	pHdr->hdrl.strl.strh.scale = LILEND4(1);
	pHdr->hdrl.strl.strh.rate = LILEND4(rate);


	long old_file_position = ftell(out);

	if (0 != fseek(out, 0L, SEEK_SET)) {
		printf("Unable to set file pointer when writing AVI header!\n");
		return false;
	}

	size_t nbw = fwrite(pHdr, sizeof(AVI_file_header), 1, out);
	//printf("%d bytes written.\n", nbw);

	if (old_file_position > 0)
		fseek(out, old_file_position, SEEK_SET);
	return true;
}

