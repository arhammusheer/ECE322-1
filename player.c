#include "player.h"

int add_card(struct player* target, struct card* new_card){
	struct hand* topHand;
	topHand = target->card_list;
	struct hand* newHand = (struct hand*)malloc(sizeof(struct hand));
	newHand->top = *new_card;
	newHand->next = topHand;
	target->card_list = newHand;
	target->hand_size++;
	return 0;
}

int remove_card(struct player* target, struct card* old_card){
	struct hand* topHand;
	topHand = target->card_list;
	struct hand* priorHand = NULL;

	while(topHand){
	    // if the suit and rank match
		if(old_card->suit == topHand->top.suit && strncmp(old_card->rank,topHand->top.rank,2)==0){
		    target->hand_size--;
		    // if the prior hand is null (Matched on the first card)
		    if(priorHand==NULL){
		        // If there is only one card in the hand
		        if(topHand->next==NULL){
		            target->card_list = NULL;
		        }
		        else{
                    topHand->top = topHand->next->top;
                    topHand->next = topHand->next->next;
		        }

		    }
		    else {
                priorHand->next = topHand->next;
            }
			break;
		}
		priorHand = topHand;
		topHand = topHand->next;
	}
	return 0;
}

char check_add_book(struct player* target){
    struct hand* topHand;
    topHand = target->card_list;
    ValidCards vcards = valid_cards_default;
    for(unsigned long long i =0; i< sizeof(vcards.ranks)/4;i++){
        int count = 0;
        // Iterate through the list searching for each occurrence of rank
        while(topHand){
            char rank[2];
            memcpy(rank, topHand->top.rank,2);
            char rank2[2];
            memcpy(rank2, vcards.ranks[i],2);
            if(strncmp(rank,rank2,2)==0){
                count++;
            }
            topHand=topHand->next;
        }
        // If a book is found, remove all cards of rank from player hand and place rank in deck
        if(count==4){
            for(unsigned long long j=0;j< sizeof(vcards.suits);j++){
                struct card newcard;
                strcpy(newcard.rank,vcards.ranks[i]);
                newcard.suit = vcards.suits[j];

                remove_card(target,&newcard);
            }
            char buffer[7];
            sprintf(buffer, "%d", i);
            strcat(target->book, buffer);
        }
        topHand = target->card_list;
    }

	return 0;
}

int search(struct player* target, char rank){
	return 0;
}

int transfer_cards(struct player* src, struct player* dest, char rank){
	return 0;
}

int game_over(struct player* target){
	return 0;
}

int reset_player(struct player* target){
	return 0;
}

char computer_play(struct player* target){
	return 0;
}

char user_play(struct player* target){
	return 0;
}

void print_user_book(struct player* target){
    ValidCards vcards = valid_cards_default;
    printf("%i\n",strlen(target->book));
    for(unsigned int i=0; i<strlen(target->book);i++){
        printf("%s ", vcards.ranks[target->book[i] - '0']);
    }
}