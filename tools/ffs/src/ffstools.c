// FFSTools - A tool for manipulating FFS partitions.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#define die(...) { printf("error: "); printf(__VA_ARGS__); puts(""); exit(1); } do {} while (0)

uint64_t gettimestamp(const char* fil) {
#ifdef _MSC_VER // Haven't been able to test with MSVC yet4. Should work, though.
	struct __stat64 buffer;
	_stat64(fil, &buffer);

	return buffer.st_mtime;
#else
	struct stat buffer;
	stat(fil, &buffer);

	return buffer.st_mtime;
#endif
}

uint64_t zero = 0;

int main(int argc, char** argv) {
	if (argc == 1) die("usage: ffstools [command] [...]");

	if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "help") || !strcmp(argv[1], "--help")) {
		puts("usage: ffstools [command] [...]");
		puts("    help - show this message");
		puts("    make - make partition at file");
		puts("    copy - copy file to partition");
		puts("    rm   - remove file from partition");
		puts("    ls   - list directory from partition");
		return 0;
	}

	if (!strcmp(argv[1], "make")) {
		if (argc < 3) die("no file specified");
		if (argc < 4) die("no size specified");

		FILE* fp = fopen(argv[2], "wb");
		if (!fp) die("failed to make file '%s'", argv[2]);

		if (fwrite("funny :)", 1, 8, fp) != 8) die("failed to write signature");

		uint64_t partsize = atoi(argv[3]);
		if (partsize < 32) die("invalid size of partition");

		if (fwrite(&partsize, 8, 1, fp) != 1) die("failed to write partition size");
		if (fwrite(&zero, 8, 1, fp) != 1) die("failed to write data stack size");
		if (fwrite(&zero, 8, 1, fp) != 1) die("failed to write entry stack size");

		for (uint64_t i = 32; i < partsize; i += 8) fwrite(&zero, 8, 1, fp);
	}

	if (!strcmp(argv[1], "copy")) {
		if (argc < 3) die("no partition specified");
		if (argc < 4) die("no source file specified");
		if (argc < 5) die("no absolute filepath specified");

		FILE* fp = fopen(argv[2], "rb+");
		if (!fp) die("failed to open file '%s'", argv[2]);

		fseek(fp, 8, SEEK_SET);

		uint64_t partsize, datasize, entrysize;
		if (fread(&partsize, 8, 1, fp) != 1) die("failed to read partition size");
		if (fread(&datasize, 8, 1, fp) != 1) die("failed to read data stack size");
		if (fread(&entrysize, 8, 1, fp) != 1) die("failed to read entry stack size");

		FILE* fp2 = fopen(argv[3], "rb+");
		if (!fp2) die("failed to open file '%s'", argv[3]);

		fseek(fp2, 0, SEEK_END);
		uint64_t filesize = ftell(fp2);
		fseek(fp2, 0, SEEK_SET);

		uint64_t datapos = 32 + datasize;
		uint64_t freesize = partsize - datasize - entrysize;
		uint64_t fileentrysize = strlen(argv[4]) + 1 + 24;

		if (freesize < filesize + fileentrysize) die("no space in partition for file");

		fseek(fp, datapos, SEEK_SET);

		for (uint64_t i = 0; i < filesize; i++) {
			uint8_t byte;
			fread(&byte, 1, 1, fp2);
			fwrite(&byte, 1, 1, fp);
		}

		fclose(fp2);

		fseek(fp, partsize - entrysize - fileentrysize, SEEK_SET);

		fwrite(argv[4], 1, strlen(argv[4]) + 1, fp);
		fwrite(&datasize, 8, 1, fp);
		fwrite(&filesize, 8, 1, fp);

		uint64_t timestamp = gettimestamp(argv[3]);
		fwrite(&timestamp, 8, 1, fp);

		fseek(fp, 16, SEEK_SET);
		
		datasize += filesize;
		entrysize += fileentrysize;
		fwrite(&datasize, 8, 1, fp);
		fwrite(&entrysize, 8, 1, fp);

		fclose(fp);
	}
}
