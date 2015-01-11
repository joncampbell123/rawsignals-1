#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

using namespace std;

#include <string>
#include <vector>

/* ------------- */
typedef struct {						/* (sizeof) (offset hex) (offset dec) */
	uint16_t 	bfType;			/* (2) +0x00 +0 */
	uint32_t 	bfSize;			/* (4) +0x02 +2 */
	uint16_t 	bfReserved1;		/* (2) +0x06 +6 */
	uint16_t 	bfReserved2;		/* (2) +0x08 +8 */
	uint32_t 	bfOffBits;		/* (4) +0x0A +10 */
} __attribute__((packed)) windows_BITMAPFILEHEADER;		/* (14) =0x0E =14 */

typedef struct {						/* (sizeof) (offset hex) (offset dec) */
	uint32_t 	biSize;			/* (4) +0x00 +0 */
	int32_t 	biWidth;		/* (4) +0x04 +4 */
	int32_t 	biHeight;		/* (4) +0x08 +8 */
	uint16_t 	biPlanes;		/* (2) +0x0C +12 */
	uint16_t 	biBitCount;		/* (2) +0x0E +14 */
	uint32_t 	biCompression;		/* (4) +0x10 +16 */
	uint32_t 	biSizeImage;		/* (4) +0x14 +20 */
	int32_t 	biXPelsPerMeter;	/* (4) +0x18 +24 */
	int32_t 	biYPelsPerMeter;	/* (4) +0x1C +28 */
	uint32_t 	biClrUsed;		/* (4) +0x20 +32 */
	uint32_t 	biClrImportant;		/* (4) +0x24 +36 */
} __attribute__((packed)) windows_BITMAPINFOHEADER;		/* (40) =0x28 =40 */
/* ------------- */

static string				dst_file;
static string				src_file;
static FILE*				src_fp = NULL;
static FILE*				dst_fp = NULL;
static int				dst_width = 800,dst_height = 525;
static double				src_rate = -1;
static unsigned char*			dst_scanline;		/* one scanline (8-bit grayscale) */

static vector<double>			csv_capture;

static void help() {
	fprintf(stderr,"ntsc-1 [options]\n");
	fprintf(stderr," -s <csv file>\n");
	fprintf(stderr," -d <bmp file>\n");
	fprintf(stderr," -w width to render to\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"the xz decompressor will be invoked if the csv file name ends in .xz\n");
}

bool read_csv() {
	char tmp[512],*scan;
	double timestamp,input1,input2;

	{
		bool use_xz = false;

		const char *s = src_file.c_str();
		assert(s != NULL);
		const char *ex = strrchr(s,'.');
		if (ex != NULL) {
			if (!strcasecmp(ex,".xz"))
				use_xz = true;
		}

		if (use_xz) {
			/* use popen() to invoke xz to decompress the file for reading */
			string cmdline = "xz -c -d -- '" + src_file + "'";
			src_fp = popen(cmdline.c_str(),"r");
		}
		else {
			src_fp = fopen(src_file.c_str(),"r");
		}

		if (!src_fp) {
			fprintf(stderr,"Failed to open source, %s\n",strerror(errno));
			return false;
		}
	}

	/* start reading! */
	while (!feof(src_fp)) {
		if (fgets(tmp,sizeof(tmp),src_fp) == NULL) break;
		scan = tmp;
		if (!isdigit(*scan) && *scan != ',') continue;

		/* timestamp, input 1 (what is of interest), input 2 */
		/* most of the samples have an empty timestamp */
		input1 = -999;
		input2 = -999;
		timestamp = -1;
		if (isdigit(*scan) || *scan == '-') timestamp = strtof(scan,&scan);
		if (*scan == ',') {
			scan++;
			if (isdigit(*scan) || *scan == '-') input1 = strtof(scan,&scan);
		}
		if (*scan == ',') {
			scan++;
			if (isdigit(*scan) || *scan == '-') input2 = strtof(scan,&scan);
		}

		/* DEBUG */
//		fprintf(stderr,"%.6f %.6f %.6f\n",timestamp,input1,input2);

		/* we care about input1 */
		if (input1 < -99) continue;

		csv_capture.push_back(input1);
	}

	fclose(src_fp);
	return true;
}

int main(int argc,char **argv) {
	char *a;
	int i;

	for (i=1;i < argc;) {
		a = argv[i++];

		if (*a == '-') {
			do { a++; } while (*a == '-');

			if (!strcmp(a,"s")) {
				a = argv[i++]; if (a == NULL) return 1;
				src_file = a;
			}
			else if (!strcmp(a,"d")) {
				a = argv[i++]; if (a == NULL) return 1;
				dst_file = a;
			}
			else if (!strcmp(a,"w")) {
				a = argv[i++]; if (a == NULL) return 1;
				dst_width = (int)strtol(a,NULL,0);
				if (dst_width < 8) return 1;
			}
			else {
				help();
				return 1;
			}
		}
		else {
			help();
			return 1;
		}
	}

	if (src_file.empty()) {
		help();
		return 1;
	}

	dst_scanline = new unsigned char[dst_width];
	if (dst_scanline == NULL) {
		fprintf(stderr,"Failed to alloc scanline\n");
		return 1;
	}

	if (!read_csv()) {
		fprintf(stderr,"Failed to read CSV\n");
		return 1;
	}

	/* round up to multiple of 4 */
	dst_width = (dst_width + 3) & (~3);

	dst_height = (csv_capture.size() + dst_width - 1) / dst_width;
	if (dst_height == 0) abort();

	dst_fp = fopen(dst_file.c_str(),"wb");
	if (dst_fp == NULL) {
		fprintf(stderr,"Cannot open dst file\n");
		return 1;
	}

	{
		windows_BITMAPFILEHEADER bmp;

		memset(&bmp,0,sizeof(bmp));
		memcpy(&bmp.bfType,"BM",2);
		bmp.bfSize = htole32(sizeof(windows_BITMAPFILEHEADER) + sizeof(windows_BITMAPINFOHEADER) + (256*4) + (dst_height * dst_width));
		bmp.bfOffBits = htole32(sizeof(windows_BITMAPFILEHEADER) + sizeof(windows_BITMAPINFOHEADER) + (256*4));
		fwrite(&bmp,sizeof(bmp),1,dst_fp);

		windows_BITMAPINFOHEADER bmi;

		memset(&bmi,0,sizeof(bmi));
		bmi.biSize = htole32(sizeof(windows_BITMAPINFOHEADER));
		bmi.biWidth = htole32(dst_width);
		bmi.biHeight = htole32(-dst_height);
		bmi.biPlanes = htole16(1);
		bmi.biBitCount = htole16(8);
		bmi.biCompression = 0;
		bmi.biSizeImage = dst_width * dst_height;
		bmi.biClrUsed = 256;
		fwrite(&bmi,sizeof(bmi),1,dst_fp);

		unsigned char colors[1024];
		for (unsigned int i=0;i < 256;i++) {
			colors[i*4 + 0] = colors[i*4 + 1] = colors[i*4 + 2] = i;
			colors[i*4 + 3] = 0;
		}
		fwrite(colors,1024,1,dst_fp);
	}

	assert(dst_height > 0);
	for (size_t h=0;h < (size_t)dst_height;h++) {
		size_t o = h * dst_width;
		size_t x = 0;
		double t;

		while (x < (size_t)dst_width && (x+o) < csv_capture.size()) {
			t = ((csv_capture[x+o] + 0.4) * 255.0 / 2.0);
			if (t < 0) t = 0;
			else if (t > 255) t = 255;
			dst_scanline[x] = (unsigned char)t;
			x++;
		}
		while (x < (size_t)dst_width)
			dst_scanline[x++] = 0;

		fwrite(dst_scanline,dst_width,1,dst_fp);
	}

	fclose(dst_fp);
	delete[] dst_scanline;
	return 0;
}

