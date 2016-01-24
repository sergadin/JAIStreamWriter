#include "stdafx.h"

#include <stdio.h>    
#include <string.h>  
#include <sys/stat.h>  
#include <fcntl.h>  
#include <stdlib.h>  
#include <io.h>   
#include <list>

#include "mmc.h"
#include "avifmt.h"
#include "avi_writer.h"



#define JPEG_DATA_SZ (sizeof(DWORD) * 2)  
#define PATH_MAX 255


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



AVIWriter::AVIWriter(LPCWSTR outputFileName)
{
	m_fileSequenceNumber = 0;
	m_width = 1;
	m_height = 1;
	m_fps = 25;
	m_baseFilename = outputFileName;
	initialize();
}


AVIWriter::~AVIWriter()
{
	if (!m_finalized)
		finalize();
}

bool AVIWriter::initialize()
{
	m_frameCount = 0;
	m_tnbw = 0;
	m_jpg_sz_64 = 0;
	m_riff_sz_64 = 0;
	m_framesList.clear();
	m_finalized = false;
	_TCHAR filename[PATH_MAX+1];
	swprintf_s(filename, _T("%s_%03d.avi"), m_baseFilename, m_fileSequenceNumber);
	_wfopen_s(&out, filename, _T("wb"));
	writeHeader();
	return true;
}

bool AVIWriter::switchToNewFile() {
	finalize();
	m_fileSequenceNumber++;
	return initialize();
}

bool AVIWriter::addFrame(const unsigned char *mem, unsigned long frame_size) {
	if (bytesRemained() < (frame_size << 4)) {
		switchToNewFile();
	}

	off_t mfsz, remnant;
	unsigned int nbw = 0; /* Number of bytes written */
	FrameData f(frame_size, 0);
	putc('0', out);
	putc('0', out);
	putc('d', out);
	putc('c', out);
	mfsz = frame_size;
	remnant = (4 - (mfsz % 4)) % 4;
	print_quartet(mfsz + remnant, out);
	f.size += remnant;

	m_frameCount++;

	if (m_frameCount == 1)
		f.offset = 4;
	else {
		FrameData& f_prev = m_framesList.back();
		f.offset = f_prev.offset + f_prev.size + 8;
	}
	fwrite(mem, 6, 1, out);
	fwrite("AVI1", 4, 1, out);
	nbw = 10;

	fwrite(mem+nbw, frame_size-nbw, 1, out);
	nbw = frame_size;

	if (remnant > 0)
	{
		fwrite(mem, remnant, 1, out);
		nbw += remnant;
	}
	m_tnbw += nbw;
	m_framesList.push_back(f);
	m_jpg_sz_64 += f.size;
	return true;
}




void AVIWriter::start(int width, int height, int fps)
{
	m_width = width;
	m_height = height;
	m_fps = fps;
	writeHeader();
}

bool AVIWriter::finalize() 
{
	if (m_finalized)
		return false;
	m_finalized = true;

	putc('i', out);
	putc('d', out);
	putc('x', out);
	putc('1', out);
	print_quartet(16 * m_frameCount, out);
	for (FrameDataIt f = m_framesList.begin(); f != m_framesList.end(); f++)
	{
		putc('0', out);
		putc('0', out);
		putc('d', out);
		putc('c', out);
		print_quartet(18, out);
		print_quartet(f->offset, out);
		print_quartet(f->size, out);
	}
	writeHeader(); // Update framecount and other header's fields
	fclose(out);
	return true;
}

bool AVIWriter::writeHeader()
{
	DWORD per_usec = 1;
	DWORD width = m_width;
	DWORD height = m_height;
	DWORD frames = (DWORD)(m_frameCount == 0L ? 1 : m_frameCount);
	off64_t jpg_sz_64 = m_jpg_sz_64,
			riff_sz_64;
	long jpg_sz = 1;
	const off64_t MAX_RIFF_SZ = 2147483648L; // 2 GB limit   
	DWORD riff_sz;
	DWORD max_bytes_per_sec = LILEND4(1000000 * (jpg_sz / frames) / per_usec);

	long old_file_position = ftell(out);
	fseek(out, 0L, SEEK_SET);

	struct AVI_list_hdrl hdrl =
	{
		{
			{ 'L', 'I', 'S', 'T' },
			LILEND4(sizeof(struct AVI_list_hdrl) - 8),
			{ 'h', 'd', 'r', 'l' }
		},
		{ 'a', 'v', 'i', 'h' },
		LILEND4(sizeof(struct AVI_avih)),
		{
			LILEND4(per_usec),
			LILEND4(1000000 * (jpg_sz / frames) / per_usec),
			LILEND4(0),
			LILEND4(AVIF_HASINDEX),
			LILEND4(frames),
			LILEND4(0),
			LILEND4(1),
			LILEND4(0),
			LILEND4(width),
			LILEND4(height),
			{ LILEND4(0), LILEND4(0), LILEND4(0), LILEND4(0) }
		},
		{
			{
				{ 'L', 'I', 'S', 'T' },
				LILEND4(sizeof(struct AVI_list_strl) - 8),
				{ 's', 't', 'r', 'l' }
			},
			{ 's', 't', 'r', 'h' },
			LILEND4(sizeof(struct AVI_strh)),
			{
				{ 'v', 'i', 'd', 's' },
				{ 'M', 'J', 'P', 'G' },
				LILEND4(0),
				LILEND4(0),
				LILEND4(0),
				LILEND4(per_usec),
				LILEND4(1000000),
				LILEND4(0),
				LILEND4(frames),
				LILEND4(0),
				LILEND4(0),
				LILEND4(0),
				LILEND2(0),
				LILEND2(0),
				LILEND2(160),
				LILEND2(120)
			},
			{ 's', 't', 'r', 'f' },
			sizeof(struct AVI_strf),
			{
				LILEND4(sizeof(struct AVI_strf)),
				LILEND4(width),
				LILEND4(height),
				LILEND4(1 + (24 << 16)),
				{ 'M', 'J', 'P', 'G' },
				LILEND4(width * height * 3),
				LILEND4(0),
				LILEND4(0),
				LILEND4(0),
				LILEND4(0)
			},
			{
				{
					{ 'L', 'I', 'S', 'T' },
					LILEND4(16),
					{ 'o', 'd', 'm', 'l' }
				},
				{ 'd', 'm', 'l', 'h' },
				LILEND4(4),
				LILEND4(frames)
			}
		}
	};


	jpg_sz_64 = m_jpg_sz_64;
	if (jpg_sz_64 == 0)
		jpg_sz_64 = 1024 * 1024; // <==================== total file size; get_file_sz(frlst);

	riff_sz_64 = sizeof(struct AVI_list_hdrl) + 4 + 4 + jpg_sz_64 + 8 * frames + 8 + 8 + 16 * frames;

	if (riff_sz_64 >= MAX_RIFF_SZ)
	{
		fprintf(stderr, "RIFF would exceed 2 Gb limit\n");
		riff_sz = 0;
		//return false;
	}

	jpg_sz = (long)jpg_sz_64;
	riff_sz = (DWORD)riff_sz_64;

	riff_sz = 0; // <======================================= FIXME; wrong calculation of riff size

	putc('R', out);
	putc('I', out);
	putc('F', out);
	putc('F', out);
	print_quartet(riff_sz, out);
	putc('A', out);
	putc('V', out);
	putc('I', out);
	putc(' ', out);
	hdrl.avih.us_per_frame = LILEND4(per_usec);
	hdrl.avih.max_bytes_per_sec = LILEND4(2 * (jpg_sz / frames) / m_fps);
	hdrl.avih.tot_frames = LILEND4(frames);
	hdrl.avih.width = LILEND4(width);
	hdrl.avih.height = LILEND4(height);
	hdrl.strl.strh.scale = LILEND4(1); // per_usec ??
	hdrl.strl.strh.rate = LILEND4(m_fps);
	hdrl.strl.strh.length = LILEND4(frames);
	hdrl.strl.strf.width = LILEND4(width);
	hdrl.strl.strf.height = LILEND4(height);
	hdrl.strl.strf.image_sz = LILEND4(width * height * 3);
	hdrl.strl.list_odml.frames = LILEND4(frames);
	fwrite(&hdrl, sizeof(hdrl), 1, out);
	putc('L', out);
	putc('I', out);
	putc('S', out);
	putc('T', out);
	print_quartet(jpg_sz + 8 * frames + 4, out);
	putc('m', out);
	putc('o', out);
	putc('v', out);
	putc('i', out);

	if (old_file_position > 0)
		fseek(out, old_file_position, SEEK_SET);
	return true;
}


bool AVIHeader::readHeader(FILE *avi_file) {
	size_t header_size = fread_s(&m_hdr, sizeof(m_hdr), 1, sizeof(m_hdr), avi_file);
	return true;
}


AVIReader::AVIReader(LPCWSTR inputFileName) {
	_wfopen_s(&m_aviFile, inputFileName, _T("rb"));
	if (!m_aviFile)
		throw "Unable to open input file.";

	m_aviHeader.readHeader(m_aviFile);
}

AVIReader::~AVIReader() {
	fclose(m_aviFile);
}


bool AVIReader::readFrame(std::vector<unsigned char>& data) {
	unsigned char buf[4];
	unsigned int frame_size = 0;
	long position = ftell(m_aviFile);

	if (4 != fread(buf, 1, 4, m_aviFile)) { // reading 00dc
		fprintf(stderr, "Unable to read frame tag prefix.\n");
		goto failure;
	}


	if (!read_quartet(&frame_size, m_aviFile)) {
		fprintf(stderr, "Unable to read frame size.\n");
		goto failure;
	}

	if (frame_size <= 0)
		goto failure;

	data.resize(frame_size);
	size_t bytes_read = fread(&data[0], 1, frame_size, m_aviFile);
	if (frame_size != bytes_read) {
		printf("Unexpected end of frame: %d bytes expected, %d read\n", frame_size, bytes_read);
		goto failure;
	}
	return true;
failure:
	fseek(m_aviFile, position, SEEK_SET);
	return false;
}
