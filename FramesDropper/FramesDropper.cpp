// FramesDropper.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <vector>
#include "../StreamCallbackSample/avi_writer.h"

void Usage() {
	fprintf(stderr, "\nUsage: FrameDropper [-rate fps] [-take k] [-start <frame number>] [-end <frame number>] source.avi [<target file name>]\n\n");
	fprintf(stderr, "\tDefault start frame: 1\n");
	fprintf(stderr, "\tDefault end frame: -1 (infinity)\n");
	fprintf(stderr, "\tDefault rate: 25 frames per secod\n");
	fprintf(stderr, "\tDefault take: 1 (copy every single frame); 3 means copying every 3rd frame.\n");
}

int _tmain(int argc, _TCHAR* argv[])
{
	std::vector<unsigned char> frame(1024*200);
	_TCHAR* sourceFileName = NULL;
	_TCHAR* targetFileName = NULL;

	int rate = 25;
	int take_one_of = 1;
	long start_frame = 1;
	long end_frame = -1;

	for (int k = 1; k < argc; k++) {
		if (wcscmp(argv[k], _T("-rate")) == 0) {
			k++;
			if (k < argc) {
				rate = _wtoi(argv[k]);
			}
		}
		else if (wcscmp(argv[k], _T("-take")) == 0) {
			k++;
			if (k < argc) {
				take_one_of = _wtoi(argv[k]);
			}
		}
		else if (wcscmp(argv[k], _T("-start")) == 0) {
			k++;
			if (k < argc) {
				start_frame = _wtol(argv[k]);
			}
		}
		else if (wcscmp(argv[k], _T("-end")) == 0) {
			k++;
			if (k < argc) {
				end_frame = _wtol(argv[k]);
			}
		}
		else {
			if (!sourceFileName)
				sourceFileName = argv[k];
			else
				targetFileName = argv[k];
		}
	}

	if (argc < 2 || !sourceFileName) {
		Usage();
		exit(1);
	}

	if (!targetFileName)
		targetFileName = _T("extraced_frame_sequence");


	AVIReader reader(sourceFileName);
	AVIWriter writer(targetFileName);
	writer.start(reader.header().width(), reader.header().height(), rate);

	if (reader.header().width() == 1 || reader.header().height() == 1) {
		printf("\n\tWARNING! Image width or height equal to 1. File not recovered?\n\n");
	}

	if (end_frame <= 0 || end_frame > reader.header().totalFrames()) {
		end_frame = reader.header().totalFrames();
	}

	wprintf(_T("\nsource file: %s\ndestination: %s\n"), sourceFileName, targetFileName);
	printf("output rate = %d frames per sec,\n take every %d'th frame, starting from %ld'th frame, ending %ld.\n\n",
		rate, take_one_of, start_frame, end_frame);
	fflush(stdout);

	long frameCount = 0, framesWritten = 0;
	while (reader.readFrame(frame)) {
		if (end_frame > 0 && frameCount+1 > end_frame)
			break;
		frameCount++;

		if ( ((frameCount-start_frame) % take_one_of != 0)
			|| frameCount < start_frame)
			continue;
		writer.addFrame(&frame[0], frame.size());
		framesWritten++;
	}
	writer.finalize();
	printf("%d frames read, %d written to the output file.\n", frameCount, framesWritten);
	return 0;
}

