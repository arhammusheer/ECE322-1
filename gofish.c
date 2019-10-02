#include <stdio.h>
#include "player.h"
#include "deck.h"
#include "gofish.h"
#include <time.h>

int which_player;
int winner;
int running;
char input_rank;


int main(int args, char* argv[]) 
{
	running = 1;
	while(running){
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
				//human player's turn

					which_player = 2; //swap turns

				//get user input
				view_hand(&user);
				print_user_book(&user);
				print_user_book(&computer);
				input_rank = user_play(&user);

					//check if opponent has the rank
					int has_rank = search(&computer, input_rank);
					if(has_rank){
						//hand over matching cards
						transfer_cards(&computer, &user, input_rank);
					}else{
						//draw a card from the deck, if it matches the requested rank, player gets another turn
						struct card* drawn_card = next_card();
						if(compare_card_rank(drawn_card, input_rank)){
							which_player = 1;
						}
						add_card(&user, drawn_card);
					}

					//check if any books were completed
					check_add_book(&user);
				}else{
					//computer player's turn

					which_player = 1; //swap turns

					//get computer input
					input_rank = computer_play(&computer);

					//check if opponent has the rank
					int has_rank = search(&user, input_rank);
					if(has_rank){
						//hand over matching cards
						transfer_cards(&user, &computer, input_rank);
					}else{
						//draw a card from the deck, if it matches the requested rank, player gets another turn
						struct card* drawn_card = next_card();
						if(compare_card_rank(drawn_card, input_rank)){
							which_player = 2;
						}
						add_card(&computer, drawn_card);
					}

					//check if any books were completed
					check_add_book(&computer);
				}

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
		if(ask_if_restart("Play again? [y/n]:\n") == 0){
			running = 0;
		}
	}
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
