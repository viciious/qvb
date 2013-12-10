#include "qvb.h"

cvector_t	vectors[MAX_IMAGE_SIZE*MAX_IMAGE_SIZE/(2*2)];
codeword_t	codebook[CODEBOOK_SIZE];

int			disttable[256];

int			distDelta = 0;
int			distDelta2 = 0;
int			maxIterations = 128;

int			randomize = 0;

int			stopIfNoSplits = 0;

int			numVectors;

void VQ_InitDistTable (void)
{
	int i;

	printf ( "Initializing dist lookup table\n" );

	for ( i = 0; i < 256; i++ ) {
		disttable[i] = i * i;
	}
}

void VQ_SetDistDelta ( int dist )
{
	distDelta = dist;
	if ( distDelta < 0 ) {
		distDelta = 0;
	} else if ( distDelta > 255 ) {
		distDelta = 255;
	}

	distDelta2 = dist * dist;
}

void VQ_SetMaximumNumberOfIterations ( int num )
{
	maxIterations = num;
	if ( maxIterations < 1 ) {
		maxIterations = 1;
	}
}

void VQ_StopIfNoSplits ( int value )
{
	stopIfNoSplits = value;
}

void VQ_EnableRandomizing ( int value )
{
	randomize = value;
}

void VQ_Init (void)
{
	static int initialized;

	if( initialized )
		return;

	initialized = 1;
	printf ( "distDelta = %i\n", distDelta );
	printf ( "maxIterations = %i\n", maxIterations );

	VQ_InitDistTable ();
}

void VQ_RGB2YCbCr( int *in, int *out )
{
	int i;
	int y[2*2], u[2*2], v[2*2];

	for( i = 0; i < 2*2; i++ ) {
		y[i] = (int)(0.299f * in[i*3+0] + 0.587f*in[i*3+1] + 0.114*in[i*3+2]);
		u[i] = (int)((in[i*3+2] - y[i]) * 0.565f);
		v[i] = (int)((in[i*3+0] - y[i]) * 0.713f);
	}

	out[0] = y[0];
	out[1] = y[1];
	out[2] = y[2];
	out[3] = y[3];
	out[4] = (u[0] + u[1] + u[2] + u[3] + 2) >> 2;
	out[5] = (v[0] + v[1] + v[2] + v[3] + 2) >> 2;

	for( i = 0; i < 2*3; i++ ) {
		if( out[i] < 0 )
			out[i] = 0;
		else if( out[i] > 255 )
			out[i] = 255;
	}
}

void VQ_QantinizeImage( byte *pic, int width, int height )
{
	int k;
	int x, y, x2, y2;
	int temp[2*2*3];
	
	for( numVectors = 0, y = 0; y < height; y += 2 ) {
		for( x = 0; x < width; x += 2, numVectors++ ) {
			for( y2 = 0; y2 < 2; y2++ ) {
				for( x2 = 0; x2 < 2; x2++ ) {
					for( k = 0; k < 3; k++ )
						temp[((y2*2) + x2)*3+k] = pic[((y+y2)*width + x+x2)*3+k];
				}
			}
			VQ_RGB2YCbCr( temp, vectors[numVectors].vec );
			vectors[numVectors].codebookentry = 0;
		}
	}

	printf( "%i input vectors\n", numVectors );
}

void VQ_InitializeCodebook (void)
{
	int i, j, k;

	printf ( "Initializing codebook\n", numVectors );

	if( randomize ) {
		srand( time (NULL) );

		for( i = 0; i < CODEBOOK_SIZE; i++ ) {
			k = rand() % numVectors;

			codebook[i].maxdist = 0;
			codebook[i].numvectors = 0;

			if( vectors[k].codebookentry ) {
				for( j = 0; j < 2*3; j++ ) {
					codebook[i].vec[j] = (vectors[k].vec[j] - (rand()%(k+1))) & 255;
					codebook[i].centroid[j] = 0;
				}
			} else {
				for( j = 0; j < 2*3; j++ ) {
					codebook[i].vec[j] = vectors[k].vec[j];
					codebook[i].centroid[j] = 0;
				}
				vectors[k].codebookentry = 1;
			}
		}
		return;
	}
	
	for( i = 0; i < CODEBOOK_SIZE; i++ ) {
		k = rand() % numVectors;

		codebook[i].maxdist = 0;
		codebook[i].numvectors = 0;

		for( j = 0; j < 2*3; j++ ) {
			codebook[i].vec[j] = vectors[k].vec[j];
			codebook[i].centroid[j] = 0;
		}
		
		vectors[k].codebookentry = 1;
	}
}

#define VQ_DistEntry(j) (disttable[(j) < 0 ? -(j) : (j)])

#if 1

#define VQ_EuclideanDistance(vec1,vec2) \
	( \
		VQ_DistEntry ((vec1)[0]-(vec2)[0])+ \
		VQ_DistEntry ((vec1)[1]-(vec2)[1])+ \
		VQ_DistEntry ((vec1)[2]-(vec2)[2])+ \
		VQ_DistEntry ((vec1)[3]-(vec2)[3])+ \
		(VQ_DistEntry ((vec1)[4]-(vec2)[4])<<2)+ \
		(VQ_DistEntry ((vec1)[5]-(vec2)[5])<<2) \
	)

#else

int VQ_EuclideanDistance( int *vec1, int *vec2 )
{
	int i;
	int dist;

	dist = 0;
	for( i = 0; i < 2*2; i++ )
		dist += VQ_DistEntry( vec1[i] - vec2[i] );

	dist += ( VQ_DistEntry( vec1[4] - vec2[4] ) << 2 );
	dist += ( VQ_DistEntry( vec1[5] - vec2[5] ) << 2 );

	return dist;
}
#endif

int VQ_Iterate( byte *pic, int width, int height )
{
	int i, j, k, np = 0;
	int bestword;
	int numchwords, numsplits;
	int dist, bestdist;
//	int outputsize;
	double t2;

	for( np = 0; np < maxIterations; np++ )	{
		for( i = 0; i < numVectors; i++ ) {
			bestword = -1;
			bestdist = 9999999;

			for( j = 0; j < CODEBOOK_SIZE; j++ ) {
				dist = VQ_EuclideanDistance( vectors[i].vec, codebook[j].vec );

				if( !dist ) {
					bestword = j;
					bestdist = 0;
					break;
				} else if( dist < bestdist ) {
					bestword = j;
					bestdist = dist;
				}
			}

			if( bestdist > codebook[bestword].maxdist ) {
				codebook[bestword].maxdist = bestdist;
				codebook[bestword].farthestvector = i;
			}

			for( k = 0; k < 2 * 3; k++ )
				codebook[bestword].centroid[k] += vectors[i].vec[k];

			vectors[i].codeword = bestword;
			codebook[bestword].numvectors++;
		}

		numchwords = 0;
		numsplits = 0;
		for( i = 0; i < CODEBOOK_SIZE; i++ ) {
			if( codebook[i].numvectors ) {
				t2 = 1.0 / (double)codebook[i].numvectors;

				for( k = 0; k < 2 * 3; k++ )
					codebook[i].centroid[k] = (int)((double)codebook[i].centroid[k] * t2 + 0.5);

				if( VQ_EuclideanDistance( codebook[i].centroid, codebook[i].vec ) > distDelta2 ) {
					numchwords++;

					for( k = 0; k < 2 * 3; k++ )
						codebook[i].vec[k] = codebook[i].centroid[k];
				}

				for( k = 0; k < 2 * 3; k++ )
					codebook[i].centroid[k] = 0;
				codebook[i].numvectors = 0;
			} else {
				bestword = 0;
				bestdist = codebook[0].maxdist;

				for( j = 1; j < CODEBOOK_SIZE; j++ ) {
					if( codebook[j].maxdist > bestdist ) {
						bestword = j;
						bestdist = codebook[j].maxdist;
					}
				}
			
				if( bestdist == 0 )
					break;

				codebook[bestword].maxdist = 0;
				bestword = codebook[bestword].farthestvector;

				for( k = 0; k < 2 * 3; k++ )
					codebook[i].vec[k] = vectors[bestword].vec[k];
				numsplits++;
			}
		}
		for( i = 0; i < CODEBOOK_SIZE; i++ )
			codebook[i].maxdist = 0;

		printf ( "\rUpdated %3i codewords, %3i splits", numchwords, numsplits );
		if( !numsplits && stopIfNoSplits )
			break;
		if( !numchwords )
			break;
	}

	printf( "\n" );

//	outputsize = 256*6+numVectors;
//	printf ( "Compressed size: %i bytes (%i kbytes)\n", outputsize, (outputsize + 1023) >> 10);
//	return outputsize;

	return 1;
}

int VQ_Compress ( byte *pic, int width, int height )
{
	VQ_QantinizeImage ( pic, width, height );

	VQ_InitializeCodebook ();

	return VQ_Iterate ( pic, width, height );
}

void VQ_Decompress( byte *out, int width, int height )
{
	int i;
	int r, g, b;
	int x, y, x2, y2;

	for( i = 0, y = 0; y < height; y += 2 ) {
		for( x = 0; x < width; x += 2, i++ ) {
			r = (int)((double)(codebook[vectors[i].codeword].vec[5]) * 1.403);
			g = (int)((double)(codebook[vectors[i].codeword].vec[4]) * (-0.344) + (double)(codebook[vectors[i].codeword].vec[5]) * (-0.714));
			b = (int)((double)(codebook[vectors[i].codeword].vec[4]) * 1.770);

			for( y2 = 0; y2 < 2; y2++ ) {
				for( x2 = 0; x2 < 2; x2++ ) {
					out[((y+y2)*width+(x+x2))*3+0] = ( byte )max( min( codebook[vectors[i].codeword].vec[(y2*2) + x2] + r, 255 ), 0 );
					out[((y+y2)*width+(x+x2))*3+1] = ( byte )max( min( codebook[vectors[i].codeword].vec[(y2*2) + x2] + g, 255 ), 0 );
					out[((y+y2)*width+(x+x2))*3+2] = ( byte )max( min( codebook[vectors[i].codeword].vec[(y2*2) + x2] + b, 255 ), 0 );
				}
			}
		}
	}
}

void VQ_ReadFrame( FILE *f, int width, int height )
{
	int i, j;
	byte c, fl1, fl2;

	if( !f )
		return;

	for( i = 0; i < CODEBOOK_SIZE; i++ ) {
		for( j = 0; j < 2*3; j++ ) {
			fread( &c, 1, sizeof(byte), f );
			codebook[i].vec[j] = c;
		}
	}

	numVectors = width * height / 4;
	for( i = 0; i < numVectors; i += 8 ) {
		fread ( &fl1, sizeof(byte), 1, f );

		if( fl1 & 1 )
			fread ( &fl2, sizeof(byte), 1, f );
		else
			fl2 = 0;

		for( j = 0; j < 8; j++ ) {
			if( i+j >= numVectors )
				break;
			if( j > 0 && (fl1 & (1<<j)) ) {
				vectors[i+j].codeword = vectors[i+j-1].codeword;
				continue;
			}

			fread( &c, 1, sizeof(byte), f );

			if( fl2 & (1<<j) )
				vectors[i+j].codeword = c + 256;
			else
				vectors[i+j].codeword = c;
		}
	}
}

void VQ_WriteCurrentImage( FILE *f )
{
	int i, j;
	int fl1, fl2;
	byte c;

	if( !f )
		return;

	for( i = 0; i < CODEBOOK_SIZE; i++ ) {
		for( j = 0; j < 2*3; j++ ) {
			c = max( min( codebook[i].vec[j], 255 ), 0 );
			fwrite( &c, 1, sizeof(byte), f );
		}
	}

	for( i = 0; i < numVectors; i += 8 ) {
		fl1 = fl2 = 0;

		for ( j = 0; j < 8; j++ ) {
			if( i+j >= numVectors )
				break;
			if( vectors[i+j].codeword >= 256 )
				fl2 |= (1<<j);
		}

		if( fl2 )
			fl1 |= (1<<0);

		for( j = 1; j < 8; j++ ) {
			if( i+j >= numVectors )
				break;
			if( vectors[i+j].codeword == vectors[i+j-1].codeword )
				fl1 |= (1<<j);
		}

		c = max( min( fl1, 255 ), 0 );
		fwrite( &c, 1, sizeof(byte), f );

		if( fl2 ) {
			c = max( min( fl2, 255 ), 0 );
			fwrite( &c, 1, sizeof(byte), f );
		}

		for( j = 0; j < 8; j++ ) {
			if( i+j >= numVectors )
				break;
			if( j > 0 && (fl1 & (1 << j)) )
				continue;

			if( fl2 & (1 << j) )
				c = max( min( vectors[i+j].codeword - 256, 255 ), 0 );
			else
				c = max( min( vectors[i+j].codeword, 255 ), 0 );

			fwrite( &c, 1, sizeof(byte), f );
		}
	}
}
