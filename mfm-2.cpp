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

static string				src_file;
static FILE*				src_fp = NULL;
static int				clock_width = -1;

static vector<double>			csv_capture;

static void help() {
	fprintf(stderr,"mfm-2 [options]\n");
	fprintf(stderr," -s <csv file>\n");
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

	if (!read_csv()) {
		fprintf(stderr,"Failed to read CSV\n");
		return 1;
	}

	return 0;
}

