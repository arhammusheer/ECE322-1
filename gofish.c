#include <stdio.h>

int which_player;
int winner;
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
			if(game_over(human)){
				winner = 1;
				break;
			}
			if(game_over(computer)){
				winner = 2;
				break;
			}


	}
	//end of game printouts here
     */

  char rank[2] = {'3', '\0'};
    char rank1[2] = {'4', '\0'};
    char rank2[2] = {'5', '\0'};
    char rank3[2] = {'6', '\0'};


  struct card C_card = create_card(rank,'C');
    struct card D_card = create_card(rank1,'D');
    struct card H_card = create_card(rank2,'H');
    struct card S_card = create_card(rank3,'S');
    add_card(&user, &C_card);
    add_card(&user, &D_card);
    add_card(&user, &H_card);
    add_card(&user, &S_card);
    transfer_cards(&user, &computer, rank[0]);
    transfer_cards(&user, &computer, rank1[0]);
    transfer_cards(&user, &computer, rank2[0]);
    transfer_cards(&user, &computer, rank3[0]);
    printf("%c", computer_play(&computer));
    check_add_book(&computer);
    print_user_book(&computer);



}

char* get_player_input(char* prompt){
	
}

int validate_player_input(char* input){
	
	
}