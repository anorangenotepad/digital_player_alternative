
/*REFS
 *
 *for basics of sdl_mixer, control flow, etc. (CodingMadeEasy)
 *https://www.youtube.com/watch?v=0TlVpiQbFiE
 *
 *for example of timer that I modified for this program
 *https://lazyfoo.net/tutorials/SDL/23_advanced_timers/index.php
 *
 */


/* COMPILE WITH:
 * gcc -o filename filename.c -lSDL2main $(pkg-config --cflags --libs sdl2 SDL2_mixer )
 *
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>



//says music, but really book...
//YOU NEED .ogg FILE TYPE FOR RESUME TO WORK CORRECTLY
//(.mp3 is not consistent with resume location... something about
// position in the stream for .mp3 vs. seconds since start of 
// file for .ogg (from sdl api)
#define MUS_PATH "my_file.ogg"

//timer struct (no class in simple C)
struct timer
{
	int start_ticks;
	int paused_ticks;
	int paused;
	int started;
};


//triggered with each pause, and writes the marker timestamp to a text file for import into audacity/hindenburg
void
marker_record_to_file(int master_timer_paused_ticks)
{


	char write_buffer[1024];

	FILE *fp;

	fp = fopen("marker_log//marker_log.txt", "a");

	if(!fp)
	{
		printf("ERR: file open for marker failed\n");
	}

	snprintf(write_buffer, sizeof write_buffer, "%d\t%d\tMARKER\n", master_timer_paused_ticks/1000, master_timer_paused_ticks/1000);

	fputs(write_buffer, fp);

	fclose(fp);

}

//also triggered with each pause
//overwrites previous stop timestamp with new timestamp each time for resume
void
stop_timestamp_record_to_file(int master_timer_paused_ticks)
{


	char write_buffer_s[1024];

	FILE *fp_s;

	fp_s = fopen("stop_location//stop_timestamp.txt", "w");

	if(!fp_s)
	{
		printf("ERR: file open for stop timestamp failed\n");
	}

	snprintf(write_buffer_s, sizeof write_buffer_s, "%d\n", master_timer_paused_ticks);

	fputs(write_buffer_s, fp_s);

	fclose(fp_s);

}

//reads value from stop_timestamp.txt
//then 'pastes' it to timestamp_from_file
//which then gets 'pasted' to the pointer
//for resume_position
//THIS IS EXTREMELY HELPFUL TO SHOW HOW TO PASS
//AND ASSIGN POINTERS FOR int!!
void
get_resume_position(int *resume_position)
{

	int timestamp_from_file;

	FILE *fp_resume;

	fp_resume = fopen("stop_location//stop_timestamp.txt", "r");

	if(!fp_resume)
	{
		printf("ERR: file open for resume failed\n");
	}

	//dont need loop because only one value...
	fscanf(fp_resume, "%d", &timestamp_from_file);
	*resume_position = timestamp_from_file;
	printf("FUNC: resume position: %d\n", *resume_position);

	//return resume_position;
	fclose(fp_resume);

}

//plays beep(s) on start-up and close of main_player()
	void
play_beep(int channel, Mix_Chunk *chunk, int loops)
{

	channel = Mix_PlayChannel(channel, chunk, loops);

	if (channel < 0) 
	{
		return;
	}
	while (Mix_Playing(channel) !=0)
	{
		SDL_Delay(200);
	}

}


//main player function!
int
main_player()
{

	/* NOTE: no bool data type in C
	 * ...well not exactly true, but easier to do it this way  
	 * 1 = true  (think ON)
	 * 0 = false (think OFF)
	 */

	//is_running is set to true by default, and just makes the 
	//main loop run as long as it is not set to false

	int is_running = 1;
	SDL_Window *window = NULL;
	SDL_Surface *window_surface = NULL;
	SDL_Surface *image_1 = NULL;
	SDL_Surface *current_image = NULL;


	image_1 = SDL_LoadBMP("mod_wiggler_cats.bmp");
	current_image = image_1;

	Mix_Music *music = NULL;

	Mix_Chunk *beep = NULL;

	//dont actually use beeps because sounds a bit off
	//Mix_Chunk *beeps = NULL;

	//this is variable for returning resume timestamp
	int resume_position = 0;

	//variable for soft volume control (system volume set with alsamixer)
	//volBASE starts at approx 10% of MIX_MAX_VOLUME, which is 128
	int volBASE = 16;


	//initialize timer elements
	struct timer master_timer;

	master_timer.start_ticks = 0;
	master_timer.paused_ticks = 0;
	master_timer.started = 0; //FALSE
	master_timer.paused = 0;  //FALSE

	//initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
		return -1;


	//Unfortunately, no window means the key events will not work...
	//when you switch over to pi, use this to prevent the window from being 'clicked away' from
	//(if window is clicked away from, keys will not work to control...
	// SCREEN_WIDTH, SCREEN_HEIGHT
	window = SDL_CreateWindow("test window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 510, 267, SDL_WINDOW_SHOWN);
	window_surface = SDL_GetWindowSurface(window);


	//initialize SDL mixer
	if(Mix_OpenAudio( 22050, MIX_DEFAULT_FORMAT, 2, 4096 ) == -1)
		return -1;

	//load music (book) and samples (beeps)
	music = Mix_LoadMUS(MUS_PATH);
	if (music == NULL)
	{
		return -1;
	}

	beep = Mix_LoadWAV("1_beep.wav");
	if (beep == NULL)
	{
		return -1;
	}


	//dont actually use beeps, becuase it sounds a bit off
	/*
	   beeps = Mix_LoadWAV("2_beep.wav");
	   if(beeps == NULL)
	   {
	   return -1;
	   }
	   */

	/*
	   This is how you play samples (different than 'music', but similar syntax)
	   -1 is the first available channel (see API)
	   if(Mix_PlayChannel(-1, beep, 0) == -1)
	   {
	   printf("ERR: start beep\n");
	   }
	   */

	//THIS SETS TO PLAY AUTOMATICALLY ON START
	//YOU WANT TO ONLY START PLAYBACK AFTER A KEY 
	//IS PRESSED SO LEAVE COMMENTED OUT
	//if ( Mix_PlayMusic( music, -1) == -1)
	//	return -1;

	//sets volume to volBASE for BOTH music (book) and effects (beep)
	//if not done, volume defaults to MAX (128) setting
	Mix_VolumeMusic(volBASE);
	Mix_Volume(-1, volBASE);

	//call to function to play beep on start
	play_beep(-1, beep, 0);

	//gets resume position from stop_timestamp.txt
	get_resume_position(&resume_position);

	//this is a variable (called event)
	//and all the event.type, etc in the loop refer to this variable
	SDL_Event event;

	//while( Mix_PlayingMusic())
	while(is_running)
	{

		while(SDL_PollEvent(&event) !=0)
		{

			if(event.type == SDL_QUIT)
				is_running = 0;

			else if(event.type == SDL_KEYDOWN)
			{
				switch(event.key.keysym.sym)
				{
					//NOTE: SDLK_0 is non 10-key 0...

					case SDLK_KP_0:
						/*this first condition checks for marker file (for resume)
						  if a marker file is found it starts playback at the point indicated
						  by the timestamp from the marker file, and updates the ticks value
						  (if you didnt update the ticks value it would start from 0 and 
						  subsequent timestamps would be wrong...)
						  */


						play_beep(-1, beep, 0);

						if(!Mix_PlayingMusic())
						{
							FILE *check_file_exists;

							if(check_file_exists = fopen("marker_log//marker_log.txt", "r"))
							{
								//printf("marker file found!\n");

								Mix_PlayMusic(music, 1);

								//need to call this here because use value for Mix_SetMusicPosition
								//get_resume_position(&resume_position);

								//recommended to rewind with Mix_RewindMusic();
								//by SDL API
								Mix_RewindMusic();

								//need to divide by 1000 because ticks are milliseconds
								//Mix_SetMusicPosition NEEDs value in seconds
								if(Mix_SetMusicPosition(resume_position/1000)==-1)
								{
									printf("Mix_SetMusicPosition: %s\n", Mix_GetError());
								}
								else
								{
									printf("POSITION %d\n",resume_position/1000);
									master_timer.started = 1;
									master_timer.paused = 0;

									//has to be minus (plus start it out at a negative)
									master_timer.start_ticks = SDL_GetTicks() - resume_position;
								}

							}
							else
							{
								Mix_PlayMusic(music, 1); //second value is number of times to play
								// -1 would be loop forever

								//starts the timer
								master_timer.started = 1;
								master_timer.paused = 0;
								master_timer.start_ticks = SDL_GetTicks();
							}
						}

						else if(Mix_PausedMusic())
							//if(Mix_PausedMusic())
						{
							Mix_ResumeMusic();

							if (master_timer.paused = 1)
							{
								//unpause the timer
								master_timer.paused = 0;
								master_timer.start_ticks = SDL_GetTicks() - master_timer.paused_ticks;
								master_timer.paused_ticks = 0;

							}

						}
						else
						{
							Mix_PauseMusic();

							//pause the timer
							master_timer.paused = 1;
							master_timer.paused_ticks = SDL_GetTicks() - master_timer.start_ticks;

							//Print time at pause
							//printf("%d\t%d\tMARKER\n", master_timer.paused_ticks, master_timer.paused_ticks);
							marker_record_to_file(master_timer.paused_ticks);
							stop_timestamp_record_to_file(master_timer.paused_ticks);

						}

						break;

					case SDLK_KP_8:
						//adjust volume up by 4
						//need conditional to prevent continued 
						//addition after hitting max
						//going over max means have to press
						//volume down key that many more times to
						//get back into range (0 to 128)

						if (volBASE >= 128)
						{
							printf("MAX VOLUME\n");
							//notification that at max
							play_beep(-1, beep, 0);
						}
						else
						{
							volBASE = volBASE + 4;
						}

						Mix_VolumeMusic(volBASE);
						Mix_Volume(-1, volBASE);
						//play_beep(-1, beep, 0);

						printf("volume: %d\n", Mix_VolumeMusic(-1));

						break;


					case SDLK_KP_2:

						//adjust volume down by 4
						//same conditional to prevent issues
						//as volume up
						if (volBASE <= 4)
						{
							printf("MIN VOL\n");
							play_beep(-1, beep, 0);
						}
						else
						{
							volBASE = volBASE - 4;
						} 

						Mix_VolumeMusic(volBASE);
						Mix_Volume(-1, volBASE);
						//play_beep(-1, beep, 0);

						printf("volume: %d\n", Mix_VolumeMusic(-1));


						break;

					case SDLK_KP_9:
						Mix_HaltMusic();

						//stop the timer
						master_timer.started = 0;
						master_timer.paused = 0;

						//kill the main loop (and the program)
						is_running = 0;
						break;

				}

			}

		}
		SDL_BlitSurface(current_image, NULL, window_surface, NULL);
		SDL_UpdateWindowSurface(window);
	}		

	//this is not the greatest, but it will reduce memory (only need 1 beep sample)
	//and make it sound a bit more consistent
	play_beep(-1, beep, 0);
	play_beep(-1, beep, 0);

	//clean up resources
	SDL_FreeSurface(image_1);
	Mix_FreeMusic(music);
	Mix_FreeChunk(beep);

	//beeps not used now
	//Mix_FreeChunk(beeps);

	//quit SDL_Mixer
	Mix_CloseAudio();

	SDL_DestroyWindow(window);
	current_image = image_1 = NULL;
	window = NULL;

	//quit mixer and sdl
	Mix_Quit();
	SDL_Quit();

	return 0;	
}


int 
main (int argc, char *argv[])
{
	//all the player stuff is handled by this function
	//and other associated (sub) functions
	main_player();

	//these varibles for system call to shut Rpi down
	char binary_path[30];
	char arg_1 [5];
	char term_command[30];

	strcpy(binary_path, "/bin/bash");
	strcpy(arg_1, "-c");
	strcpy(term_command, "poweroff");

	/* NOTE ABOUT execl COMMAND BELOW THIS COMMENT:
	   you probably want to leave this commented out, since even trying to 
	   kill with htop will result in the Pi shutting off

	   probably not an issue for the actual units because
	   you can just open file browser and get what you need off prior to closing program
	   but a problem for me on the development Pi as
	   power goes down every time the program ends...

	   in case you do need to turn off after it has been uncommented and compiled:
	   1) open another window
	   2) open .c file using vim
	   3) comment out execl() line
	   4) recompile with gcc
	   5) close program and turn pi off
	   6) next time pi is turned on, it will not shut down after program close
	   */

	execl(binary_path, binary_path, arg_1, term_command, NULL);

	return 0;

}

