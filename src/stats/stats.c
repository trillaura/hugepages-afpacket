#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <time.h>

#define INDECES 18
#define PRECISION 4

static int RUNS = 0;
const char *names[] = {
	"delay",
	"page-faults", 
	"dTLB-loads", 
	"dTLB-load-misses", 
	"dTLB-stores", 
	"dTLB-store-misses", 
	"cycles", 
	"instructions", 
	"cache-misses", 
	"cache-references", 
	"time_s",
	"upackets",
	"kpackets",
	"bytes",
	"dropped",
	"maxcpucycles",
	"avgcpucycles",
	"nblocks"
};  

void get_filename(char *filename) {
	char *p = "plot_";
	char *s = "_synt.dat";
	sprintf(filename,"%s%s", p, s);
}

double roundTo(double x, unsigned int n) {
	double fac = pow(10, n);
	return round(x*fac)/fac;
}

int writeData(int option, int delay, int nblocks, long double *avg,
	       	long double *var, long double *stdev) {
	
	char filename[50];
	get_filename(filename);

	FILE *f = fopen(filename, "a");
	if (!f) {
		perror("fopen");
		return 1;
	}
	int i;

	fprintf(f, "#");
	fprintf(f, "\toption");
	fprintf(f, "\tdelay");
	for (i = 1; i < INDECES-1; ++i) {
		fprintf(f, "\t%s-avg", names[i]);
		fprintf(f, "\t%s-stdev", names[i]);
		fprintf(f, "\t%s-var", names[i]);
	}
	fprintf(f, "nblocks");
	fprintf(f, "\n");
	
	switch (option) {
		case 25:	
			fprintf(f, "\tHUGETLB");
			break;
		case 26:
			fprintf(f, "\tTRANSHUGE");
			break;
		case 27:
			fprintf(f, "\tGFPNORETRY");
			break;
		case 28:
			fprintf(f, "\tVZALLOC");
			break;
		case 29:
			fprintf(f, "\tGFPRETRY");
			break;
		case 5:
			fprintf(f, "\tRX_RING");
			break;
		default:
			perror("invalid option");
			return 1;
	}

	fprintf(f, "\t%d", delay);
	for (i = 1; i < INDECES-1; ++i) {
		fprintf(f, "\t%Lf", avg[i]);
		fprintf(f, "\t%Lf", stdev[i]);
		fprintf(f, "\t%Lf", var[i]);
	}
	fprintf(f, "\t%d", nblocks);
	fprintf(f, "\n");

	fclose(f);

	return 0;
}

int parse_events(int idx, const char *file, double values[][RUNS]) {
	char line[256];
	FILE *f = fopen(file, "r");
	if (!f) {
		perror("fopen");
		return 1;
	}

	int i = 0;
	while (fgets(line, sizeof(line), f) != NULL) {
		values[i][idx] = strtof(line, NULL); 
		++i;
	}
	fclose(f);
	return 0;
}

int parse_opt(char *filename) {
	int option = 0;
	char *tmp;
	tmp = strtok(filename, "_");
	while (tmp) {
		option = atoi(tmp);
		tmp = strtok(NULL, "_");
	}
	return option;
}

int main(int argc, char **argv) {
	
	if (argc < 3) {
		printf("Usage: <filename> <#files>\n \
			\t - filename of the file containing the list of filenames\n");
		return 1;
	}

	int NUM_FILES = atoi(argv[2]);
	RUNS = NUM_FILES;

	double values[INDECES][RUNS];
	long double avg[INDECES];
	long double stdev[INDECES];
	long double var[INDECES];
	int option = 5;
	int delay, nblocks;
	int err = 0;
	int i, r;
	char filename[NUM_FILES][256];
	char line[256];
	FILE *list;
       
	list = fopen(argv[1], "r");
	if (!list) {
		perror("open");
		return 1;
	}

	for ( i = 0; i < NUM_FILES; ++i) {
		fgets(line, sizeof(line), list);
		strcpy(filename[i], strtok(line, "\n" ));
	}
	fclose(list);

	for (i = 0; i < NUM_FILES; i++) {
		char *tmp = strdup(filename[i]);
		option = parse_opt(filename[i]);
		err = parse_events(i, tmp, values);
		if (err) {
			perror("parse_events");
			return 1;
		}
	}

	delay = values[0][0];
	nblocks = values[INDECES-1][0];
	for (i = 1; i < INDECES; ++i) {
		long double tmp = 0.0;
		avg[i] = 0.0;
		var[i] = 0.0;
		stdev[i] = 0.0;
		/* iterative statistics  */
		for (r = 1; r <= RUNS; ++r) {	
			tmp = avg[i];
			avg[i] = avg[i] + (values[i][r-1] - avg[i])/r;
			var[i] = var[i] + 
				(values[i][r-1] - avg[i])*(values[i][r-1]-tmp);
		}
		var[i] = var[i]/(RUNS-1);
		stdev[i] = sqrt(var[i]);
	}

	writeData(option, delay, nblocks, avg, var, stdev);

	return 0;
}
