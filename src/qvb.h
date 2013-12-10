#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <time.h>

typedef unsigned char byte;

#define MAX_IMAGE_SIZE	1024

#define QVB_FRAMERATE	30
#define QVB_HEADER		(('1'<<24)+('B'<<16)+('V'<<8)+'Q')

// files.c
int FS_FileLength ( FILE *f );
int FS_LoadFile ( char *name, void **buffer );

// images.c
int LoadTGA (char *name, byte **pic, int *width, int *height);
void WriteTGA (char *picname, byte *pic, int width, int height);

// vq.c

#define CODEBOOK_SIZE	512

typedef struct
{
	// block size is defined with BLOCK_SIZE, each consisting of
	// BLOCK_SIZE*BLOCK_SIZE RGB pixels
	int		codeword;
	int		codebookentry;
	int		vec[2*3];		// YCbCr encoded as 4 1 1
} cvector_t;

typedef struct
{
	int		maxdist;
	int		farthestvector;
	int		vec[2*3];

	int		numvectors;
	int		centroid[2*3];
} codeword_t;

void VQ_Init (void);
void VQ_SetDistDelta ( int dist );
void VQ_SetMaximumNumberOfIterations ( int num );
void VQ_StopIfNoSplits ( int value );
void VQ_EnableRandomizing ( int value );
int VQ_Compress ( byte *pic, int width, int height );
void VQ_Decompress ( byte *pic, int width, int height );
void VQ_ReadFrame ( FILE *f, int width, int height );
void VQ_WriteCurrentImage ( FILE *f );
