#include "qvb.h"

/*
================
FS_FileLength
================
*/
int FS_FileLength ( FILE *f )
{
	int		pos;
	int		end;

	pos = ftell ( f );
	fseek ( f, 0, SEEK_END );
	end = ftell ( f );
	fseek ( f, pos, SEEK_SET );

	return end;
}

/*
================
FS_LoadFile
================
*/
int FS_LoadFile ( char *name, void **buffer )
{
	FILE *f;
	int length;

	if ( !name ) {
		return -1;
	}

	*buffer = NULL;

	f = fopen ( name, "rb" );
	if ( !f ) {
		return -1;
	}

	length = FS_FileLength ( f );
	if ( !buffer ) {
		return length;
	}
	if ( !length ) {
		return 0;
	}

	*buffer = malloc ( length );
	fread ( *buffer, length, 1, f );
	fclose ( f );

	return length;
}
