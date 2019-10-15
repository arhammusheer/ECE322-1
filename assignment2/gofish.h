#ifndef GOFISH_H
#define GOFISH_H

/*
   Define any prototype functions
   for gofish.h here.
*/
/*
	Function: ask_if_restart
	
	prompts the user if they would like to restart the game. Reads 1 character. 
	Y/y restarts the game and N/n does not restart. Reprompts the user if the 
	input is none of those characters.
	
	Parameters: string prompt to display to the user
	Returns: 1 if the user wants to restart, 0 if they do not
*/
int ask_if_restart(char* prompt);

/*
	Function: init_game
	
	initializes and resets the game state. Resets the player structs, recreates and shuffles the deck,
	deals out a new hand of 7 cards, and reseeds the random number generator.
	
	Parameters: none
	Returns: none
*/
void init_game();

/*
	Function: player_turn
	
	runs a player's turn. Asks for user input, moves cards around based on the input, checks
	for new books, and changes the turn if appropriate
	
	Parameters: player1: user whose turn it is. player2: user whose turn it is not
	Returns: none
*/
void player_turn(struct player* player1, struct player* player2);

#endif
