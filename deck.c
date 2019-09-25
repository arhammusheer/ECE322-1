#include "deck.h"


int shuffle(){
	
	//add every card to the deck
	int index = 0;
	for(int st = 0; st <= 3; st++){
		for(int rk = 1; rk <= 13; rk++){
			index = st*13 + rk-1;
			char* r = get_rank(rk);
			deck_instance.list[index].rank[0] = *r[0];
			deck_instance.list[index].rank[1] = *r[1];
		}
		deck_instance.list[index].suit = get_suit(st);
	}
	
	//shuffle the deck
	
	return 0;
}


int deal_player_cards(struct player* target){
	
	return 0;
}


struct card* next_card(){

	
}


size_t deck_size(){
	
	
}

char get_suit(int s){
	switch(s){
	case 0: return 'C';
	case 1: return 'D';
	case 2: return 'H';
	case 3: return 'S';
	default: return '\0';
	}
}

char* get_rank(int r){
	char r[2];
	switch(r){
	case 1: 
		r[0] = 'A';
		r[1] = '\0';
		break;
	case 2:
		r[0] = '2';
		r[1] = '\0';
		break;
	case 3:
		r[0] = '3';
		r[1] = '\0';
		break;
	case 4:
		r[0] = '4';
		r[1] = '\0';
		break;
	case 5:
		r[0] = '5';
		r[1] = '\0';
		break;
	case 6:
		r[0] = '6';
		r[1] = '\0';
		break;
	case 7:
		r[0] = '7';
		r[1] = '\0';
		break;
	case 8:
		r[0] = '8';
		r[1] = '\0';
		break;
	case 9:
		r[0] = '9';
		r[1] = '\0';
		break;
	case 10:
		r[0] = '1';
		r[1] = '0';
		break;
	case 11:
		r[0] = 'J';
		r[1] = '\0';
		break;
	case 12:
		r[0] = 'Q';
		r[1] = '\0';
		break;
	case 13:
		r[0] = 'K';
		r[1] = '\0';
		break;
	default:
		r[0] = '\0';
		r[1] = '\0';
		break;
	}
	return r;
}