#include "qvb.h"
#include <SDL.h>

SDL_Surface *screen;

void init_video (int width, int height, int full_screen)
{
	Uint32 vidflags = 0;
	
	if ( SDL_Init (SDL_INIT_VIDEO) < 0 ) {
		printf ( "Couldn't initialize SDL: %s\n", SDL_GetError() );
		exit ( 1 );
	}
	
	atexit ( SDL_Quit );
	
	SDL_EventState ( SDL_KEYUP, SDL_IGNORE );
	SDL_EventState ( SDL_MOUSEMOTION, SDL_IGNORE );
	
	vidflags = SDL_SWSURFACE;
	if ( full_screen ) vidflags |= SDL_FULLSCREEN;
	
	if ( !(screen = SDL_SetVideoMode (width, height, 32, vidflags)) ) {
		printf ( "Couldn't set video mode: %s\n", SDL_GetError () );
		exit ( 1 );
	}
	
	SDL_Delay ( 500 );
}

void display ( byte *buf, int width, int height )
{
	byte *out = (char *)screen->pixels + screen->offset;
	SDL_Rect dest;
	int x, y, rs = screen->format->Rshift/8, gs = screen->format->Gshift/8,
	bs = screen->format->Bshift/8;

	if(SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
	
	for (y = 0; y < height; y++)
	{
		for (x = 0; x < width; x++)
		{
#if 1
			out[(x*4)+rs] = max ( min(buf[(y*width+x)*3+0] + 0, 255), 0 );
			out[(x*4)+gs] = max ( min(buf[(y*width+x)*3+1] + 0, 255), 0 );
			out[(x*4)+bs] = max ( min(buf[(y*width+x)*3+2] + 0, 255), 0 );
#else
			unsigned char r, g, b;
			unsigned char ri, gi, bi;
			unsigned char DitherMatrix[4][4] =
			{
				{ 0, 8, 2, 10 },
				{ 12, 4, 14, 6 },
				{ 3, 11, 1, 9 },
				{ 15, 7, 13, 5 }
			};
			unsigned char Ditherval;

			Ditherval = DitherMatrix[x&3][y&3];
			r = buf[(y*width+x)*3+0] >> 2;
			g = buf[(y*width+x)*3+1] >> 2;
			b = buf[(y*width+x)*3+2] >> 2;

			ri = (r >> 3); if (ri != 0 && ((r&7)<<1) <= Ditherval ) ri--;
			gi = (g >> 3); if (gi != 0 && ((g&7)<<1) <= Ditherval ) gi--;
			bi = (b >> 3); if (bi != 0 && ((b&7)<<1) <= Ditherval ) bi--;

			out[(x*4)+rs] = (ri<<5);
			out[(x*4)+gs] = (gi<<5);
			out[(x*4)+bs] = (bi<<5);
#endif
		}

		out += screen->pitch;
	}

	if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);

	dest.x = 0;
	dest.y = 0;
	dest.w = width;
	dest.h = height;

	SDL_UpdateRects(screen, 1, &dest);
}

int main ( int argc, char **argv ) 
{
	int header, br;
	int width, height;
	int framenum;
	FILE *f;
	byte *buf;
	SDL_Event event;
	Uint32 start_tm, cur_tm, display_time;

	if ( argc < 2 ) {
		printf ( "usage: qvbview filename\n" );
		return 0;
	}

	f = fopen ( argv[1], "rb" );
	if ( !f ) {
		printf ( "could not open %s\n", argv[1] );
		return 1;
	}

	fread ( &header, sizeof(int), 1, f );

	if ( header != QVB_HEADER ) {
		printf ( "%s is not a qvb file\n", argv[1] );
		fclose ( f );
		return 1;
	}

	fread ( &width, sizeof(int), 1, f );
	fread ( &height, sizeof(int), 1, f );
//	fread ( &numframes, sizeof(int), 1, f );

	init_video ( width, height, 0 );

	buf = malloc ( width * height * 3 );

#if 1
	VQ_ReadFrame ( f, width, height );
	VQ_Decompress ( buf, width, height );
#endif

	framenum = 1;
	while ( 1 ) {
#if 0
		if ( !feof (f) ) {
			VQ_ReadFrame ( f, width, height );
			VQ_Decompress ( buf, width, height );
		}
#endif

		if ( SDL_PollEvent(&event) ) {
			switch ( event.type ) 
			{
				case SDL_KEYDOWN:
				case SDL_MOUSEBUTTONDOWN:
				case SDL_MOUSEBUTTONUP:
					br = 1;
					break;
				default:
					br = 0;
					break;
			}

			if ( br ) {
				break;
			}
		}

		if ( framenum == 1 ) {
			start_tm = SDL_GetTicks ();
		}

		display_time = (1000 * (Uint32)framenum)/QVB_FRAMERATE;
		cur_tm = SDL_GetTicks();
		while(cur_tm - start_tm < display_time)
		{
			SDL_Delay (10);
			cur_tm = SDL_GetTicks();
		}

		display ( buf, width, height );

#if 0
		if ( !feof (f) ) {
			framenum++;
		}
#endif
	}

	free ( buf );
	fclose ( f );

	return 0;
}
