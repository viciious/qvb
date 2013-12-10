#include "qvb.h"

FILE *output;
int outputWidth = 0, outputHeight = 0;

int QVB_CompressFrame ( char *name, int doutput, int silent )
{
	byte *pic;
	int start, end;
	int width, height;
	float fseconds;
	int minutes, seconds;

	printf ( "Loading %s\n", name );

	if ( !LoadTGA (name, &pic, &width, &height) ) {
		if ( !silent ) {
			printf ( "qvb: could not load %s\n", name );
		}
		return 0;
	}

	if ( !outputWidth ) {
		outputWidth = width;
	}
	if ( !outputHeight ) {
		outputHeight = height;
	}

	VQ_Init ();

	start = clock();
	VQ_Compress ( pic, width, height );

	if ( doutput ) {
		byte *out = malloc ( width * height * 3 );

		VQ_Decompress ( out, width, height );

		WriteTGA ( "out.tga", out, width, height );

		free ( out );
	}

	free ( pic );

	end = clock();

	fseconds = ( end - start ) / 1000.0f;
	minutes = (int)(fseconds / 60);
	seconds = (int)fseconds - minutes * 60;

	printf ( "Time: %is (%im : %2is)\n", (int)fseconds, minutes, seconds );

	return 1;
}

void QVB_StartCompression ( char *name )
{
	int header = QVB_HEADER;

	output = fopen ( name, "wb" );
	if ( !output ) {
		printf ( "Error opening %s\n", name );
		exit ( 1 );
	}

	fwrite ( &header, sizeof(int), 1, output );
	fwrite ( &outputWidth, sizeof(int), 1, output );
	fwrite ( &outputHeight, sizeof(int), 1, output );

//	fwrite ( &numframes, sizeof(int), 1, output );
}

void QVB_WriteCurrentFrame (void)
{
	VQ_WriteCurrentImage ( output );
}

void QVB_EndCompression (void)
{
	fclose ( output );
}

int main ( int argc, char **argv )
{
	int i, f;
	char name[1024];
	int doutput = 0;

	if ( argc < 3 ) {
		printf ( "usage: qvb filename output [-options]\n" );
		return 0;
	}
	for ( i = 2; i < argc; i++ ) {
		if ( !strcmp (argv[i], "-dist") ) {
			if ( i+1 >= argc ) {
				printf ( "-dist: insufficient number of parameters\n" );
				return 1;
			}

			VQ_SetDistDelta ( atoi (argv[++i]) );
		} else if ( !strcmp (argv[i], "-maxit") ) {
			if ( i+1 >= argc ) {
				printf ( "-maxit: insufficient number of parameters\n" );
				return 1;
			}

			VQ_SetMaximumNumberOfIterations ( atoi (argv[++i]) );
		} else if ( !strcmp (argv[i], "-doutput") ) {
			doutput = 1;
		} else if ( !strcmp (argv[i], "-fast") ) {
			VQ_StopIfNoSplits ( 1 );
		} else if ( !strcmp (argv[i], "-random") ) {
			VQ_EnableRandomizing ( 1 );
		}
	}

	if ( doutput ) {
		printf ( "Debug output turned on\n" );
	}

	printf ( "\n" );

	sprintf ( name, "%s\0", argv[1] );
//	sprintf ( name, "avidemo000.tga" );
	if ( !QVB_CompressFrame (name, doutput, 0) ) {
		return 1;
	}

	QVB_StartCompression ( argv[2] );

	QVB_WriteCurrentFrame ();

	f = 1;
	while (1)
	{
		name[7] = '0' + f/100;
		name[8] = '0' + (f%100)/10;
		name[9] = '0' + ((f%100)%10);

		if ( !QVB_CompressFrame (name, doutput, 1) ) {
			break;
		}

		QVB_WriteCurrentFrame ();
		f++;
	}

	QVB_EndCompression ();

	return 0;
}
