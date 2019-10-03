#include <stdio.h>
#include "player.h"
#include "deck.h"
#include "gofish.h"
#include <time.h>

int which_player;
int winner;
int running;
struct player* current_player;
struct player* opposite_player;


int main(int args, char* argv[]) 
{
	running = 1;
	while(running){
		
		//initialize the game state
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

           current_player  = which_player == 1 ? &user : &computer;
           opposite_player = which_player == 1 ? &computer : &user;
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
		
		//ask if want to play again
		running = ask_if_restart("Play again? [y/n]:\n");
	}
}

void player_turn(struct player* player1, struct player* player2){
    char input_rank;
    int start_turn = which_player;
    // Always view the users hand on turn and both books
    view_hand(&user);
	print_user_book(&user);
	print_user_book(&computer);


	if(which_player == 1 && player1->hand_size)
		input_rank = user_play(player1);
	else if(which_player ==2){
        printf("Player 2's turn, enter a rank: ");
        input_rank = computer_play(player1);
        printf("%-2s\n", get_complete_char(input_rank));
    }
	else{
	    input_rank = '0';
	}

	//change turn
	which_player = which_player % 2 + 1;

	//check if opponent has the rank
	int has_rank = search(player2, input_rank);
	if(has_rank && player1->hand_size){
	    printf("   - %s has ",player1->name);
	    print_hand_card_of_rank(player1, input_rank);
	    printf("\n");

        printf("   - %s has ",player2->name);
        print_hand_card_of_rank(player2, input_rank);
        printf("\n");

		//hand over matching cards
		transfer_cards(player2, player1, input_rank);
        //Player gets another turn
        which_player = which_player % 2 + 1;
	}else{
		//draw a card from the deck, if it matches the requested rank, player gets another turn
		struct card* drawn_card = next_card();
		if(compare_card_rank(drawn_card, input_rank)){

			which_player = which_player % 2 + 1;
		}
		add_card(player1, drawn_card);
		// player has already changed so check if it is player 2 turns
        if(start_turn==1)
            printf("   - Go Fish, %s draws %2s%c\n",player1->name, drawn_card->rank, drawn_card->suit);
        else
            printf("   - Go Fish, %s draws a card\n",player1->name);
	}

	//check if any books were completed
	if(check_add_book(player1))
	    printf("   - %s Books %2s\n",player1->name, get_complete_char(player1->book[player1->book_size-1]));
	if(start_turn==which_player)
	    printf("   - %s gets another turn\n",player1->name);

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
	static char reply;
    printf("%s", prompt);
    // Scan char from stdin
    scanf("%c", &reply);
    while ( getchar() != '\n' );
    if(reply == 'y' || reply == 'Y'){
		return 1;
	}
	if(reply == 'n' || reply == 'N'){
		return 0;
	}
	return ask_if_restart("Please enter [y/n]\n");
}
