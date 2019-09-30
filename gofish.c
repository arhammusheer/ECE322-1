#include <stdio.h>

int which_player;
struct player* human;
struct player* computer;
char input_rank;


int main(int args, char* argv[]) 
{
	which_player = 1; //human goes first
	while(1){
			//game runs in here
			
			//steps:
			//1. get player1 input and validate 
			//2. check if player2 has cards
			//3. give cards from player2's hand
			//4. give card from the deck if applicable
			//5. if card from deck is same rank as input, still player1's turn
			//6. check for books
			//7. repeat. swap player1 with player2 when turns end
			
			/*
			
			if(which_player == 1){
				which_player = 2;
				input_rank = user_play();
				int has_rank = search(computer, input_rank);
				if(has_rank){
					transfer_cards(computer, human, input_rank);
				}else{
					struct card* drawn_card = next_card();
					if(compare_card(drawn_card, input_rank)){
						which_player = 1;
					}
				}
				check_add_book(human);
			}else{
				which_player = 1;
				input_rank = computer_play();
				int has_rank = search(human, input_rank);
				if(has_rank){
					transfer_cards(human, computer, input_rank);
				}else{
					struct card* drawn_card = next_card();
					if(compare_card(drawn_card, input_rank)){
						which_player = 2;
					}
				}
				check_add_book(computer);
			}
			
			
			Step 2:
			int has_rank = 
			
			
			*/
			
			
	}
	//end of game printouts here
  fprintf(stdout, "Put your code here.");
}

char* get_player_input(char* prompt){
	
}

int validate_player_input(char* input){
	
	
}