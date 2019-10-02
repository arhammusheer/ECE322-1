#include <stdio.h>
#include "player.h"
#include "deck.h"
#include "gofish.h"
#include <time.h>

int which_player;
int winner;
int running;
char input_rank;
struct player* current_player;
struct player* opposite_player;


int main(int args, char* argv[]) 
{
	running = 1;
	while(running){
		init_game();
		while(1){
			//game runs in here

			//steps:
			//0. Add 7 cards into player 1 and player 2 hands
			//1. get player1 input and validate
			//2. check if player2 has cards
			//3. give cards from player2's hand
			//4. give card from the deck if applicable
			//5. if card from deck is same rank as input, still player1's turn
			//6. check for books
			//7. repeat. swap player1 with player2 when turns end

			if(which_player == 1){
				current_player = &user;
				opposite_player = &computer;
			}else{
				current_player = &computer;
				opposite_player = &user;
			}
			
			player_turn(current_player, opposite_player);
			

			//check if there is a winner
			if(game_over(&user)){
				winner = 1;
				break;
			}
			if(game_over(&computer)){
				winner = 2;
				break;
			}

		}
		//end of game printouts here

		if(winner == 1){ //player wins
			printf("You win!");
		}else{ //computer wins
			printf("Computer wins!");
		}
		running = ask_if_restart("Play again? [y/n]:\n");
	}
}

void player_turn(struct player* player1, struct player* player2){
	
	if(which_player == 1)
		view_hand(player1);
	print_user_book(player1);
	print_user_book(player2);
	if(which_player == 1)
		input_rank = user_play(player1);
	else
		input_rank = computer_play(player1);
	
	//change turn
	which_player = which_player % 2 + 1;

	//check if opponent has the rank
	int has_rank = search(player2, input_rank);
	if(has_rank){
		//hand over matching cards
		transfer_cards(player2, player1, input_rank);
	}else{
		//draw a card from the deck, if it matches the requested rank, player gets another turn
		struct card* drawn_card = next_card();
		if(compare_card_rank(drawn_card, input_rank)){
			which_player = which_player % 2 + 1;
		}
		add_card(player1, drawn_card);
	}

	//check if any books were completed
	check_add_book(player1);
	
}

void init_game(){
	//reset players
	reset_player(&user);
	reset_player(&computer);

	// Init time and seed the random number generator
	srand(time(NULL));
	rand();
	which_player = 1; //human goes first

	//Name each player
	strcpy(user.name, "Player 1");
	strcpy(computer.name, "Player 2");
	shuffle();
	// Deal cards to both user and computer player
	deal_player_cards(&user);
	deal_player_cards(&computer);
}

int ask_if_restart(char* prompt){
	char reply;
    printf("%s", prompt);
    // Scan char from stdin
    scanf(" %c", reply);
    if(reply == 'y' || reply == 'Y'){
		return 1;
	}
	if(reply == 'n' || reply == 'N'){
		return 0;
	}
	return ask_if_restart("Please enter [y/n]\n");
}
