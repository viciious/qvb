#include "qvb.h"

/*
=========================================================

TARGA LOADING

=========================================================
*/

typedef struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;


/*
=============
LoadTGA
=============
*/
int LoadTGA (char *name, byte **pic, int *width, int *height)
{
	int		columns, rows, numPixels, row_inc;
	byte	*pixbuf;
	int		row, column;
	byte	*buf_p;
	byte	*buffer;
	int		length;
	TargaHeader		targa_header;
	byte			*targa_rgba;
	byte tmp[2];

	*pic = NULL;

	//
	// load the file
	//
	length = FS_LoadFile (name, (void **)&buffer);
	if (!buffer || length <= 0)
	{
		return 0;
	}

	buf_p = buffer;

	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;
	
	tmp[0] = buf_p[0];
	tmp[1] = buf_p[1];
	targa_header.colormap_index = *((short *)tmp);
	buf_p+=2;
	tmp[0] = buf_p[0];
	tmp[1] = buf_p[1];
	targa_header.colormap_length = *((short *)tmp);
	buf_p+=2;
	targa_header.colormap_size = *buf_p++;
	targa_header.x_origin = *((short *)buf_p);
	buf_p+=2;
	targa_header.y_origin = *((short *)buf_p);
	buf_p+=2;
	targa_header.width = *((short *)buf_p);
	buf_p+=2;
	targa_header.height = *((short *)buf_p);
	buf_p+=2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;

	if (targa_header.width > MAX_IMAGE_SIZE || targa_header.height > MAX_IMAGE_SIZE) {
		printf ("LoadTGA: Maximum %ix%i images are supported\n", MAX_IMAGE_SIZE, MAX_IMAGE_SIZE);
		free ( buffer );
		exit ( 1 );
	}

	if (targa_header.image_type!=2 
		&& targa_header.image_type!=10) {
		printf ("LoadTGA: Only type 2 and 10 targa RGB images supported\n");
		free ( buffer );
		exit ( 1 );
	}

	if (targa_header.colormap_type !=0 
		|| (targa_header.pixel_size!=32 && targa_header.pixel_size!=24)) {
		printf ( "LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n");
		free ( buffer );
		exit ( 1 );
	}
	if (targa_header.pixel_size != 24) {
		printf ("LoadTGA: Only 24 bit targa RGB images supported\n");
		free ( buffer );
		exit ( 1 );
	}

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	if (width)
		*width = columns;
	if (height)
		*height = rows;

	targa_rgba = malloc (numPixels*3);
	*pic = targa_rgba;

	if (targa_header.id_length != 0)
		buf_p += targa_header.id_length;  // skip TARGA image comment

    if (targa_header.attributes & 0x20)
    {
        pixbuf = targa_rgba;
        row_inc = 0;
	}
    else
    {
        pixbuf = targa_rgba + (rows - 1)*columns*3;
        row_inc = -columns*3*2;
    }

	if (targa_header.image_type==2) {  // Uncompressed, RGB images
		for( row = 0; row < rows; row++ ) {
			for( column = 0; column < columns; column++ ) {
				unsigned char red,green,blue;

				switch (targa_header.pixel_size) {
					case 24:
						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						break;
				}
			}
			pixbuf += row_inc;
		}
	}
	else if (targa_header.image_type==10) {   // Runlength encoded RGB images
		unsigned char red,green,blue,packetHeader,packetSize,j;

		for( row = 0; row < rows; row++ ) {
			for( column = 0; column < columns; ) {
				packetHeader= *buf_p++;
				packetSize = 1 + (packetHeader & 0x7f);

				if (packetHeader & 0x80) {        // run-length packet
					switch (targa_header.pixel_size) {
						case 24:
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							break;
					}
	
					for(j=0;j<packetSize;j++) {
						*pixbuf++=red;
						*pixbuf++=green;
						*pixbuf++=blue;
						column++;
						if( column == columns ) { // run spans across rows
							column = 0;
							if (row < rows - 1)
								row++;
							else
								goto breakOut;
							pixbuf += row_inc;
						}
					}
				}
				else {                            // non run-length packet
					for(j=0;j<packetSize;j++) {
						switch (targa_header.pixel_size) {
							case 24:
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								*pixbuf++ = red;
								*pixbuf++ = green;
								*pixbuf++ = blue;
								break;
						}
						column++;
						if( column == columns ) { // run spans across rows
							column = 0;
							if (row < rows-1)
								row++;
							else
								goto breakOut;
							pixbuf += row_inc;
						}
					}
				}
			}
			pixbuf += row_inc;
			breakOut:;
		}
	}

	free (buffer);

	return length;
}

/* 
================== 
WriteTGA
================== 
*/  
void WriteTGA (char *picname, byte *pic, int width, int height)
{
	byte		*buffer;
	int			i, c;
	FILE		*f;

// 
// find a file name to save it to 
// 
	buffer = malloc(width*height*3 + 18);
	memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = width&255;
	buffer[13] = width>>8;
	buffer[14] = height&255;
	buffer[15] = height>>8;
	buffer[16] = 24;	// pixel size
	buffer[17] = 0x20;

	// swap rgb to bgr
	c = 18+width*height*3;
	for (i=18 ; i<c ; i+=3)
	{
		buffer[i] = pic[i-18+2];
		buffer[i+1] = pic[i-18+1];
		buffer[i+2] = pic[i-18];
	}

	f = fopen (picname, "wb");
	fwrite (buffer, 1, c, f);
	fclose (f);
	free (buffer);

	printf ("Wrote %s\n", picname);
} 
