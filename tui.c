#include "tui.h"

#include <ncurses.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h> 

#include "sound.h"
#include "keyboard.h"

static bool debug = true;

//prototype 
/* visual file browser fn */ 
void fileBrowse(char buf[128])
{
	/* @param buf[128] : buffer that will be populated with the user selection path */

	nodelay(stdscr, FALSE); //WAIT for user input
	int x, y, offset = 0;	//screen attribs
	DIR *dir;
	struct dirent *ent;
	strcpy(buf, PATCH_ROOT);
	getmaxyx(stdscr, y, x);
	strcpy(buf, PATCH_ROOT);
	file_iterate(buf, dir, ent, offset, y-2);
}

/* recursive helper function for fileBrowse*/
static void file_iterate(char name[512], DIR *dir, struct dirent *ent, int offset, int max_y)
{
	/* @param name : char buffer to be populated
	   @param dir : dir object used in dirent
	   @praram ent : " used in dirent
	   @param offset : what display page we're on
	   @param max_y : the max y dimension of the terminal */
	int selection = 0;
	while (true)
	{
		clear();
		refresh();
		attron(A_UNDERLINE);
		printw("Inside dir: %s\n", name);
		attroff(A_UNDERLINE);
		if ((dir = opendir(name)) != NULL)
		{
			int i = 0, j = 0;
			char *n = NULL;
			int ch = 0;
			while ((ent = readdir(dir)) != NULL && j < max_y)
			{
				if (! (i < offset))
				{
					if (selection == j) 
					{
						attron(A_STANDOUT);
						n = ent->d_name;
					}
					printw("%s\n", ent->d_name);
					j++;
					attroff(A_STANDOUT);
				}
				else 
					i++;
			}
			ch = getch();
			switch(ch)
			{
				case 10:
					strcat(name, n);
					strcat(name, "/");
					file_iterate(name, dir, ent, offset, max_y);
					return;
				case KEY_DOWN:
					selection++;
					if (selection > max_y)
					{
						selection = 0;
						offset += max_y;
					}
					break;
				case KEY_UP:
					selection--;
					if (selection < 0)
					{
						selection = 0;
						offset = offset > 0 ? offset - max_y : 0;
					}
					break;
				case 8: //backspace
					if (strcmp(PATCH_ROOT, name) != 0)
					{
						strcat(name, "../");
						file_iterate(name, dir, ent, offset, max_y);
						return;
					}
					break;
			}
		}
		else
		{
			char c = name[0];
			int i = 0;
			while (c != '\0')
				c = name[i++];
			name[i-2] = '\0';
			printf("%s\n", name);
			//getch();
			return;
		}
	}
}

void mainMenu()
{
	initscr();
	raw();
	if (initSDL() == -1)
	{
		printw("Unable to init SDL\n");
		endwin();
	}
	int ch = 0;
	float tempo = 120;

	clear();
	refresh();

	keypad(stdscr, TRUE);
	//menu relative vars
	int select = 0, //current selected item
		chan = 0, 	//current selected channel
		d_iter = 0; 
		
	clear();
	refresh();
	while (true)
	{
		echo();
		clear();
		refresh();
		attron(A_UNDERLINE);
		printw("Tr99 - v%f | Tempo %d | \n", VERSION_NUMBER, tempo);
		attroff(A_UNDERLINE);
		if (d_iter == 0) attron(A_STANDOUT);
		printw("\tPattern Selection\n");
		attroff(A_STANDOUT);
		if (d_iter == 1) attron(A_STANDOUT);
		printw("\tLoad Sound Bank\n");
		attroff(A_STANDOUT);
		if (d_iter == 2) attron(A_STANDOUT);
		printw("\tGlobal Settings\n");
		attroff(A_STANDOUT);
		ch = getch();
		switch (ch)
		{
			case KEY_UP:
				d_iter = (d_iter > 0) ? d_iter-1 : d_iter;
				break;
			case KEY_DOWN:
				d_iter = (d_iter < 2) ? d_iter+1 : d_iter;
				break;
			case YES:
				switch (d_iter)
				{
					case 0:
						patternEditor(tempo, ch, d_iter);
						break;
					case 1:
						break;
					case 2:
						break;

				}
				break;
			case DELETE:
				endwin();
				return;
				break;
		}
	}
	endwin();

}

void patternEditor(float tempo, int ch, int d_iter)
{
	clear();
	refresh();
	Channel *mix[16];
	Sample *bank[16];
	for (int i = 0; i < 16; i++)
		mix[i] = NULL;
	printw("Loading bank\n"); getch();
	loadSampleBank("1.bank", bank);
	for (int i = 0; i < 3; i++)
		printw("Bank sound %d: %s\n", i, bank[i]->fname);
	printw("Loading sequence\n");getch();
	importSequence("2.track", mix, bank);
	char *bank_name = "1.bank";
	
	d_iter = 0, ch = 0;
	int select = 0, chan = 0, bank_selected = 0, sound = 0;

	enum modes {SELECT, COMPOSE, EDIT};
	enum modes mode = SELECT;

	while (ch != 113)
	{
		bool empty = false;
		clear();
		refresh();
		attron(A_UNDERLINE);
		printw("Tempo: %d bpm | 'p' - play/pause\n\n", (int)tempo);//getch();
		attroff(A_UNDERLINE);
		for (int i = 0; i < 16; i++)
		{
			if (mode == SELECT && select == i)
				attron(A_BOLD);
			if (mix[chan] != NULL && mix[chan]->pattern[i] != NULL)
				printw("[*]");
			else
				printw("[ ]");
			attroff(A_BOLD);
		}//getch();
		printw("\n\nThe current Sample Bank is: %s\nMode: %d\n", bank_name, mode);
		if (mode == COMPOSE)
		{
			printw("The current selected sample is: %s", mix[chan]->sound->fname);
			printw("Press TAB to exit...");
		}
		else if (mode == EDIT)
		{
			printw("Press step to edit..\n");
			if (mix[chan]->pattern[select] == NULL)
				printw("This is an empty step!\n");
			else
			{
				printw("Sample:\t%s\n", mix[chan]->sound->fname);
				printw("Velocity:\t%f\n", mix[chan]->pattern[select]->vol);
				printw("Hold:\t%%100\n");
				printw("Pitch:\t~0\n");
				printw("Treble:\t1\t\n");
				printw("Mid:\t1\n");
				printw("Bass:\t1\n");
			}
		}
		else if (mode == SELECT)
		{
			printw("Select channel to edit\n");
			for (int i = 0; i < 16; i++)
			{
				printw("%d:\t", i);
				if (mix[i] != NULL)
					printw("%s\n", mix[i]->sound->fname);
				else
					printw("empty\n");
			}			
		}

		ch = handleFuckingButtons(ch);
		//printw("Handled buttons, ch = %d\n", ch);//getch();
		bool skip = true;
		if (ch == CHANGEMODE)
		{
			switch (mode)
			{
				case COMPOSE:
					mode = SELECT;
					select = 0;
					break;
				case SELECT:
					//printw("Here\n");getch();
					mode = EDIT;
					printw("Mode: %d", mode);getch();
					//skip = false;
					break;
				case EDIT:
					mode = COMPOSE;
					skip = false;
					break;
			}
		}
		else if (ch == PLAY)
		{
			playingDisplay(tempo, ch, mix);
		}
		if (!skip)
		{
			clear();
			refresh();
			printw("Entering compose mode, select bank patch (1-16)\n");
			for (int i = 0; i < 16; i++)
			{
				printw("%d:\t", i);
				if (mix[i] != NULL)
					printw("%s\n", mix[i]->sound->fname);
				else
					printw("empty\n");
			}
			ch = handleFuckingButtons(ch);
			if (ch > 15 || bank[ch] == NULL)
			{
				if (ch != CHANGEMODE)
				{
					printw("Cannot load empty bank slot, returning to select mode..\n");
					sleep(3);
				}
				mode = SELECT;
				select = 0;
			}
			else
			{
				chan = ch;
			}
		}
		else
		{
			if (mode == SELECT && ch >= 0 && ch < 16)
				chan = ch;
			else if (mode = COMPOSE && ch >= 0 && ch < 16)
			{
				if (mix[chan]->pattern[ch] == NULL)
				{
					mix[chan]->pattern[ch] = initStep(1.0);
				}
				else
				{
					destroyStep(mix[chan]->pattern[ch]);
				}
			}
			else if (mode == EDIT && ch >= 0 && ch < 16)
			{
				select = ch;
			}
		}
		//printw("Channel = %d\n", chan);getch();
	}
}

static int handleFuckingButtons(int ch)
{
	int select;
	ch = getch();
	switch(ch)
	{
		case B1:
			select = 0;
			break;
		case B2:
			select = 1;
			break;
		case B3:
			select = 2;
			break;
		case B4:
			select = 3;
			break;
		case B5:
			select = 4;
			break;
		case B6:
			select = 5;
			break;
		case B7:
			select = 6;
			break;
		case B8:
			select = 7;
			break;
		case B9:
			select = 8;
			break;
		case B10:
			select = 9;
			break;
		case B11:
			select = 10;
			break;
		case B12:
			select = 11;
			break;
		case B13:
			select = 12;
			break;
		case B14:
			select = 13;
			break;
		case B15:
			select = 14;
			break;
		case B16:
			select = 15;
			break;
		default:
			return ch;
	}
	return select;
}

void playingDisplay(float tempo, int ch, Channel *mix[16])
{
	nodelay(stdscr, TRUE);
	int i = 0, prev = i;
	//printf("Before float operation\n");
	float steps = (60/(tempo) * 1000000) / 4;
	clear();
	refresh();
	ch = 0;
	//printf("Before assigning args\n;");
	struct p_args pa;
	//pa.seq = seq;
	for (int j = 0; j < 16; j++)
	{
		pa.mix[j] = mix[j];
	}

	pa.ch = ch;
	pa.i = i;
	pa.steps = steps;
	pthread_t player;
	//printf("Before pthread create\n;");
	pthread_create(&player, NULL, &playSequence, (void *)&pa);
	while (pa.ch != PAUSE)
	{
		//playStep(&(seq[i]));
		//steps = ;
		if (pa.i != prev)
		{
			clear();
			refresh();
			prev = pa.i;
			printw("Tempo: %d\tSteps: %d\n", (int)tempo, (int)pa.steps);
			for (int j = 0; j < 16; j++)
			{
				if ((j) % 4 == 0 && j != 15 || j == 0)
					attron(A_STANDOUT);
				if (pa.i == j)
					printw("[*]");
				else
					printw("[ ]");
				if ((j) % 4 == 0 && j != 15 || j == 0)
					attroff(A_STANDOUT);
			}
		}

		//printf("\n");
		pa.ch = getch();
		//i = (i < 15) ? i+1 : 0;
		//usleep((int)steps);
		switch(pa.ch)
		{
			case KEY_UP:
				tempo++;
				break;
			case KEY_DOWN:
				tempo--;
				break;
		}
		pa.steps = (60/(tempo) * 1000000) / 4;
	}
	usleep(steps);
	//destroy_p_args(pa);
	nodelay(stdscr, FALSE);

	return;
}
