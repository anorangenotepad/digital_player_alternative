/* REFS
 *
 * same as fake_player_simple
 *
 * for basic sdl structure
 * https://www.youtube.com/watch?v=0TlVpiQbFiE
 *
 * for timer
 * https://lazyfoo.net/tutorials/SDL/23_advanced_timers/index.php
 *
 */


/* NOTES:

   compile with:
   gcc -Wall -g -o list_gen_tts_sdl list_gen_tts_sdl.c -lflite_cmu_us_kal16 -lflite_usenglish -lflite_cmulex -lflite -lm -lSDL2main $(pkg-config --cflags --libs sdl2 SDL2_mixer)


   NOTE (20210908)
   WE WILL NEED GLOBAL VARIABLES FOR THE FILES AFTER A BOOK HAS BEEN SELECTED
   TO SEND TO SUB FUNCTIONS FOR MARKERS, ETC.

 */



#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
//isspace() function lives in ctype.h
#include <ctype.h>
//flite.h needed for tts
#include <flite/flite.h>

//sdl for audio and key control
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

//system calls need unistd.h
#include <unistd.h>


//needed to initialize(?) flite voice synth
cst_voice *register_cmu_us_kal16();

#define MAX_FILENAME_LENGTH 14 

//global variables (easier to do it this way than pass the variables all the time)
char base_file_path[50] = "//path//to//remove//media//";

char book_dir[70];
char stop_dir[70];
char marker_dir[70];

char books[10] = "books";
char stop_locations[20] = "stop_locations";
char marker_locations[20] = "marker_locations";

char marker_filename[100];
char stop_filename[100];

//will be used as a wrapper for selected_book returned from book_chooser_menu()
//and the file extention to build the audio, marker, and stop filename and path  to be
//sent to main_sdl_player()
char selected_audio_file[200];
char selected_marker_file[200];
char selected_stop_file[200];

//this sets a flag that will be detected by a conditional after 
//resources have been freed
//SLDK_KP_MINUS and SDLK_KP_7 do essentially the same thing
//BUT! the former sets the flag to 1 which calls the book_chooser_menu()
//function and the later simply quits the program
int go_back_to_book_chooser_menu;


int main_sdl_interface();

//struct for linked list
struct book_list
{
  char filename [MAX_FILENAME_LENGTH];
  struct book_list *next;
};
struct book_list *start = NULL;


//timer struct (no class in simple C)
//need for creating marker and stop locations
struct timer
{
  int start_ticks;
  int paused_ticks;
  int paused;
  int started;
};



//actually mounts or unmounts the cartridge based on the condition specified 
//in confirm_cart_mount_status
//NOTE: calls execl() in a child process to prevent premature termination
int
book_cart_mount_unmount(char *passed_command)
{  

  char binary_path[10];
  char arg_1[3];

  strcpy(binary_path, "/bin/bash");
  strcpy(arg_1, "-c");


  /*NOTE about vfork() vs fork()
    both of the blocks below (vfork() one and fork() one)
    work

    The reason i need to 'fork' is because execl() actually 'takes over/hijacks'
    the program when it is called, and it wont be possible to return to main or
    call any other functons after execl() is called.

    So, i need to call a child process to run just the execl statement in, and
    then return to the parent process when finished in order to continue the program

    as far as fork() vs vfork():
    according to stack overflow, the difference is that vfork() does not
    copy the original programs memory and does not create a new address space
    instead it 'borrows' the address space of the parent process 
    temporarily until it completes

    I understand this to mean that vfork() is 'more compact' and uses less
    resources

    That being said, for this program its not such a big deal, so i decided
    to just use fork(), but left vfork() in so i have a workign example for 
    future use
  */

  /* 
  if (!vfork())
  {
    execl(binary_path, binary_path, arg_1, passed_command, NULL);
  }
  */

  
  pid_t childpid = fork();

  if (childpid < 0)
  {
    printf("ERR: cannot create child process for execl\n");
    exit(0);
  }
  else if (childpid == 0)
  {
    execl(binary_path, binary_path, arg_1, passed_command, NULL);
  }
  

  return 0;

}

//checks to see whether the cart is mounted or not 
//and passes necessary command to book_cart_mount_unmount()
//based on cart condition and when in the program it is called
//(basically mounts each time at program start and unmounts each
// time prior to program end... see main())
int 
confirm_cart_mount_status()
{

  char udisks_mount_cart[35] = "udisksctl mount -b /dev/sdX1";
  char udisks_unmount_cart[35] = "udisksctl unmount -b /dev/sdX1";

 
  DIR* cart_mount_dir = opendir("//path//to//remove//media//dir");

  if (!cart_mount_dir)
  {
    printf("cart not mounted. mounting...\n");
    book_cart_mount_unmount(udisks_mount_cart);
  }
  else if (cart_mount_dir)
  {
    printf("cart mounted. unmounting...\n");
    closedir(cart_mount_dir);
    book_cart_mount_unmount(udisks_unmount_cart);
  }
  /*
  else if (cart_mount_dir && (go_back_to_book_chooser_menu = 1))
  {
    printf("cart mounted. going back to chooser menu...\n");
    closedir(cart_mount_dir);
    //selected_book = "placeholder";
    //book_chooser_menu_sdl(selected_book);
  }
  */

  else
  {
    printf("ERR: cart mount/unmount error!\n");
    exit(0);
  }

  //you need a delay in here, otherwise odd race condition occurs
  SDL_Delay(500);

  return 0;

}

//basically does what it says
//these are used to locate book files, and write/read marker and stop locations
//(writes to global variables)

void
create_finished_file_paths()
{
  strcpy(book_dir, base_file_path);
  strcat(book_dir, books);

  strcpy(stop_dir, base_file_path);
  strcat(stop_dir, stop_locations);

  strcpy(marker_dir, base_file_path);
  strcat(marker_dir, marker_locations);

}

//function that inserts filenames of files in target directory 
//into the linked list
void
create_book_list(char *filename_string)
{
  struct book_list *temp, *ptr;
  temp = (struct book_list*) malloc (sizeof(struct book_list));

  if (temp == NULL)
  {
    printf("ERR: out of memory space\n");
    exit(0);
  }

  strcpy(temp->filename, filename_string);
  temp->next = NULL;

  if(start == NULL)
  {
    start = temp;
  }
  else
  {
    ptr = start;
    while (ptr->next != NULL)
    {
      ptr = ptr->next;
    }
    ptr->next = temp;
  }
}


//shaves last four chars (i.e. .ogg) off audio files for easier processing
char*
format_string(char *target_string)
{
  size_t len = strlen(target_string);
  if (len >= 4)
  {
    target_string[len-4] = 0;
  }

  return target_string;
}


//checks to see if files (marker, stop) exist, and creates them
//if they dont (displays message if they do...)
int
create_marker_files(char *txt_filename_string)
{
  FILE *fp;

  fp = fopen(txt_filename_string, "r");

  if (fp != NULL)
  {
    printf("file already exists: %s\n", txt_filename_string);
    fclose(fp);
  }
  else
  {
    fp = fopen(txt_filename_string, "a");
    //fprintf(fp, "%s\n", txt_filename_string);
    fclose(fp);
  }

  return 0;
}


void
create_marker_filenames(char *filename_string)
{

  snprintf(marker_filename, 
          sizeof(marker_filename), 
          "%s//%s_mark.txt", 
          marker_dir, 
          filename_string);

  snprintf(stop_filename, 
          sizeof(stop_filename), 
          "%s//%s_stop.txt", 
          stop_dir, 
          filename_string);

  //printf("%s\n%s\n", marker_filename, stop_filename);

  create_marker_files(marker_filename);
  create_marker_files(stop_filename);

}


//function that actually scans the directory for file names within
//these filenames are passed to create_book_list() to build
//the linked list
int
directory_scan(char *book_dir)
{
  struct dirent **directory_contents_list;
  int dcl, filter1, filter2;

  char filename_string[MAX_FILENAME_LENGTH];
  char this_dir[4] = ".";
  char upper_dir[4] = "..";

  dcl = scandir(book_dir, &directory_contents_list, NULL, alphasort);

  if (dcl < 0)
  {
    perror("ERR: scandir issue");
    return -1;
  }
  else
  {
    while (dcl--)
    {
      strncpy(filename_string, directory_contents_list[dcl]->d_name, MAX_FILENAME_LENGTH);

      //this is how to print discovered directory names at this stage
      //printf("%s\n", directory_contents_list[dcl]->d_name);
      //OR, you could just do (wont print beyond char limit though)
      //printf("%s\n", filename_string);


      //this is basic method to send to linked list
      //create_book_list(filename_string);


      // i added 'filters' to keep "." and ".." from
      // being added to the linked list
      //easier to keep them from being added than to delete
      // linked list entries later

      filter1 = strcmp(filename_string, this_dir);
      filter2 = strcmp(filename_string, upper_dir);

      if (filter1 != 0 && filter2 != 0)
      {

        //shaves off file extention (i.e. .ogg)
        format_string(filename_string);

        create_book_list(filename_string);
        create_marker_filenames(filename_string);
      }

      free(directory_contents_list[dcl]);
    }
    free(directory_contents_list);
  }
  return 0;
}

//just reads text passed to it!
void
text_to_speech_read(char *passed_text)
{
  //this is all for initializing(?) flite needed stuff
  cst_voice *v;
  flite_init();
  v = register_cmu_us_kal16(NULL);

  flite_text_to_speech(passed_text, v, "play");

}


//new chooser loop
//this is where you can cycle through the linked list (with 10-key minus)
//the current book gets assigned to the selected_book variable on each pass
//when you get to the one you want, hit 10-key plus to exit the loop 
//and returm the selected book to the main 
char*
book_chooser_menu_sdl(char *selected_book)
{
  char no_book [50] = "Error. Book list reed error";

  struct book_list *ptr;

  int chooser_loop_running = 1; //1 means 'true' (0 means 'false')
  
  //these are used for the error handling when 
  // the + key is pressed and a book has not been
  // selected
  char checker [20] = "placeholder";
  int book_menu_filter;

  if (start == NULL)
  {
    printf("ERR: linked list is empty\n");
    text_to_speech_read(no_book);
    exit(0);
  }
  else
  {
    ptr = start;
  }

  char chooser_message[150] = "Book selection menu. Please choose a book";
  text_to_speech_read(chooser_message);

  SDL_Event event;

  while (chooser_loop_running)
  {
    while (SDL_PollEvent(&event) != 0)
    {
      if (event.type == SDL_QUIT)
      {
        chooser_loop_running = 0;
      }
      else if(event.type == SDL_KEYDOWN)
      {
        switch(event.key.keysym.sym)
        {
          case SDLK_KP_MINUS:
            if (ptr != NULL)
            {
              strcpy(selected_book, ptr->filename);
              text_to_speech_read(selected_book);
              ptr = ptr->next;
            }
            else 
            {
              ptr = start;
            } 
            printf("keep going\n");
            break;
          case SDLK_KP_PLUS:
	    //error handling that compares value of selected_book with checker
	    //since selected_book is set to "placeholder" by default
	    //if it is still "placeholder" after this step, that means a book has
	    // not been chosen, so the message will be read
	    // and loop will continue until a book has been selected
	    book_menu_filter = strcmp(selected_book, checker);
	    if (book_menu_filter != 0)
	    {
              printf("stop!\n");
              chooser_loop_running = 0;
	    }
	    else
	    {
              char no_book [80] = "book not selected. please choose a book";
	      text_to_speech_read(no_book);
	    }
            break;
        }
      }
    }
  }

  return selected_book;

}

//triggered with each pause, and writes the marker timestamp to a text file for import into audacity/hindenburg
void
marker_record_to_file(int master_timer_paused_ticks)
{


  char write_buffer[1024];

  FILE *fp;

  fp = fopen(selected_marker_file, "a");

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

  fp_s = fopen(selected_stop_file, "w");

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

  fp_resume = fopen(selected_stop_file, "r");

  if(!fp_resume)
  {
    printf("ERR: file open for resume failed HERE!\n");
  }

  //dont need loop because only one value...
  fscanf(fp_resume, "%d", &timestamp_from_file);
  *resume_position = timestamp_from_file;
  printf("FUNC: resume position: %d\n", *resume_position);

  //return resume_position;
  fclose(fp_resume);

}

//plays beep(s) on start-up and close of playback function
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


//main playback function
//main player function!
int
main_sdl_player()
{

	/* NOTE: no bool data type in C
   * ...well not exactly true, but easier to do it this way  
	 * 1 = true  (think ON)
	 * 0 = false (think OFF)
	 */

	//is_running is set to true by default, and just makes the 
	//main loop run as long as it is not set to false
	//in current config, pressing 2 will set to false and exit the loop
	
	int is_running = 1;

  
	Mix_Music *music = NULL;

  Mix_Chunk *beep = NULL;

  //use this as error handling to not play in file does not exist
  //forgot to fclose in original program though!! thats bad!
  FILE *check_file_exists;

  //variable for soft volume control and default vol setting
  //sdl doesnt seem to like variable names with underscores
  //so made this one camelCase
  //volBase starts at approx. 10% of MIX_MAX_VOLUME, which is 128
  int volBase = 16;

  //this is variable for returning resume timestamp
  int resume_position = 0;

  //initialize timer elements
  struct timer master_timer;

  master_timer.start_ticks = 0;
  master_timer.paused_ticks = 0;
  master_timer.started = 0; //FALSE
  master_timer.paused = 0;  //FALSE

	//initialize SDL
	if ( SDL_Init(SDL_INIT_AUDIO) < 0)
		return -1;


	//initialize SDL mixer
	if(Mix_OpenAudio( 22050, MIX_DEFAULT_FORMAT, 2, 4096 ) == -1)
		return -1;

	//load music (book) and samples (beeps)
	music = Mix_LoadMUS(selected_audio_file);
	if (music == NULL)
  {
    return -1;
  }

  beep = Mix_LoadWAV("1_beep.wav");
  if (beep == NULL)
  {
    return -1;
  }

  
 
  //sets volume to volBase for BOTH music (book) and effects (beep)
  //if not done, volume defaults to MAX (128) setting
  //-1 for Mix_Volume sets all channels (i think...)
  Mix_VolumeMusic (volBase);
  Mix_Volume (-1, volBase);


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
					
          
          case SDLK_KP_1:
            /*this first condition checks for marker file (for resume)
              if a marker file is found it starts playback at the point indicated
              by the timestamp from the marker file, and updates the ticks value
              (if you didnt update the ticks value it would start from 0 and 
               subsequent timestamps would be wrong...)
            */
            play_beep(-1, beep, 0);

            if(!Mix_PlayingMusic())
            {

              if(check_file_exists = fopen(selected_marker_file, "r"))
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
                //this needs to be closed here or will cause issues
                fclose(check_file_exists);
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

          case SDLK_KP_9:

            //adjust volume up by 4
            //need conditional to prevent continued 
            //addition after hitting MAX
            //going over MAX means have to press 
            //volume down key that many more times 
            //to get back in range (0 -128)

            if (volBase >= 128)
            {
              printf("MAX VOLUME\n");
              //notification that at max
              play_beep(-1, beep, 0);
            }
            else
            {
              volBase = volBase + 4;
            }

            Mix_VolumeMusic(volBase);
            Mix_Volume(-1, volBase);

            printf("volume: %d\n", Mix_VolumeMusic(-1));

            break;

         case SDLK_KP_3:

            //adjust volume down by 4
            //same conditional chain to prevent 
            //issues as volume up

            if (volBase <= 4)
            {
              printf("MIN VOL\n");
              play_beep(-1, beep, 0);
            }
            else
            {
              volBase = volBase - 4;
            }

            Mix_VolumeMusic(volBase);
            Mix_Volume(-1, volBase);

            printf("volume: %d\n", Mix_VolumeMusic(-1));

            break;

				  case SDLK_KP_MINUS:
            if (!Mix_PlayingMusic())
            {
              printf("playback not started\n");
            }
            else
            {
              Mix_HaltMusic();
              //stop the timer
              master_timer.started = 0;
              master_timer.paused = 0;
            }

            
            //kill the main loop (and the program)
						is_running = 0;

            go_back_to_book_chooser_menu = 1;

						break;

					case SDLK_KP_7:
						if (!Mix_PlayingMusic())
            {
              printf("playback not started\n");
            }
            else
            {
              Mix_HaltMusic();
              //stop the timer
              master_timer.started = 0;
              master_timer.paused = 0;
            }
            

            //kill the main loop (and the program)
						is_running = 0;

            go_back_to_book_chooser_menu = 0;

						break;

				}

			}

		}
	}		

  //this is not the greatest, but it will reduce memory (only need 1 beep sample)
  //and make it sound a bit more consistent
  play_beep(-1, beep, 0);
  play_beep(-1, beep, 0);

	//clean up resources
	Mix_FreeMusic(music);
  Mix_FreeChunk(beep);
  

	//quit SDL_Mixer
	Mix_CloseAudio();


  //quit mixer and sdl
	Mix_Quit();


  

	return 0;	
}

int
book_chooser_menu_switch()
{
  //if (go_back_to_book_chooser_menu = 1)
  //{
    main_sdl_interface();
  //}
  //else
  //{
    //printf("not returning to main_sdl_interface...\n");
  //}

  return 0;
}


  int
main_sdl_interface()
{

  //and used as the container for the selected book
  //needs to be initialized to a default value or will hang...
  char selected_book[20] = "placeholder";

  
  
  //initialize sdl variable, etc.
  SDL_Window *window = NULL;
  SDL_Surface *window_surface = NULL;
  SDL_Surface *image_1 = NULL;
  SDL_Surface *current_image = NULL;

  image_1 = SDL_LoadBMP("mod_wigger_cats.bmp");
  current_image = image_1;

  if (SDL_Init(SDL_INIT_VIDEO) < 0)
  {
    return -1;
  }

  //set window parameters
  window = SDL_CreateWindow("main_window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 510, 267, SDL_WINDOW_SHOWN);
  window_surface = SDL_GetWindowSurface(window);

  SDL_BlitSurface(current_image, NULL, window_surface, NULL);
  SDL_UpdateWindowSurface(window);


  //book chooser(returns selected book from linked list)
  book_chooser_menu_sdl(selected_book);
  //printf("selected book is: %s\n", selected_book);

  //builds the filenames and paths to be sent to playback_menu_sdl()  
  snprintf(selected_audio_file, 
           sizeof selected_audio_file, 
           "%s//%s.ogg", 
           book_dir, 
           selected_book);
  
  printf("selected audio file is: %s\n", selected_audio_file);

  snprintf(selected_marker_file, 
           sizeof selected_marker_file, 
           "%s//%s_mark.txt", 
           marker_dir, 
           selected_book);

  printf("selected marker file is: %s\n", selected_marker_file);

  snprintf(selected_stop_file, 
           sizeof selected_stop_file, 
           "%s//%s_stop.txt", 
           stop_dir, 
           selected_book);

  printf("selected stop file is: %s\n", selected_stop_file);


  main_sdl_player();

  //this just cleans everything up after finished
  SDL_FreeSurface(image_1);

  SDL_DestroyWindow(window);
  current_image = image_1 = NULL;
  window = NULL;

  SDL_Quit();

  if(go_back_to_book_chooser_menu == 0)
  {
    printf("done!\n");
    //book_chooser_menu_switch();
  }
  else if (go_back_to_book_chooser_menu == 1)
  {
    book_chooser_menu_switch();
  }

  return 0;

}


//main...
int
main()
{
  

  //variables to pass to tts 
  
  char start_message[150] = "Remote review station. Starting up.";
  char cart_mounting[50] = "attempting to mount cartridge";
  char power_down[50] = "powering down";

  //go_back_to_book_chooser_menu = 1;

  text_to_speech_read(start_message);

  confirm_cart_mount_status();
  text_to_speech_read(cart_mounting);

  SDL_Delay(500);

  create_finished_file_paths();

  directory_scan(book_dir);

  main_sdl_interface();

  SDL_Delay(500);

 
  confirm_cart_mount_status();
  text_to_speech_read(power_down);

	//no execl() call in this program, but plan to add in 
	//a similar way to fake_player_simple


  return 0;
}

