#include "player.h"


int view_hand(struct player* target){
    printf("%s\'s Hand -", target->name);
    struct hand* topHand;
    topHand = target->card_list;
    while(topHand){
		printf("%c%c%c ", topHand->top.rank[0], topHand->top.rank[1], topHand->top.suit);
        topHand = topHand->next;
    }
    printf("\n");
    return 0;
}


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
            if(compare_ranks(topHand->top.rank, vcards.ranks[i])){
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
            target->book[target->book_size] = vcards.ranks[i][0];
            target->book_size++;
            return vcards.ranks[i][0];
        }
        topHand = target->card_list;
    }

	return 0;
}

int search(struct player* target, char rank){
    struct hand* topHand;
    topHand = target->card_list;
    while(topHand){
        if(compare_card_rank(&topHand->top, rank)){
            return 1;
        }
        topHand = topHand->next;
    }
	return 0;
}

int transfer_cards(struct player* src, struct player* dest, char rank){
    if(!search(src, rank)){
        return 0;
    }
    char rnk[2];
    memcpy(rnk, get_complete_char(rank),2);
    struct card C_card = create_card(rnk,'C');
    struct card D_card = create_card(rnk,'D');
    struct card H_card = create_card(rnk,'H');
    struct card S_card = create_card(rnk,'S');
    struct card card_ar[4] = {C_card, D_card, H_card, S_card};

    struct card current_card;
    struct hand* topHand;
    topHand = src->card_list;
    for(int i=0;i<4;i++){
        current_card = card_ar[i];
        while(topHand){
            if(compare_card(&topHand->top, &current_card)){
                remove_card(src, &current_card);
                add_card(dest, &current_card);
                break;
            }
            topHand = topHand->next;
        }
        topHand = src->card_list;
    }

	return 0;
}

int game_over(struct player* target){
    if(strlen(target->book)==7)
        return 1;
	return 0;
}

int reset_player(struct player* target){
    memset(target->book, '\0', 7);
    target->card_list = NULL;
    target->hand_size = 0;
    target->book_size = 0;

	return 0;
}

char computer_play(struct player* target){
    if(target->hand_size == 0)
        return 0;
    size_t r = rand();
    r = r%(target->hand_size);
    struct hand* topHand = target->card_list;
    for(size_t i=0;i<r;i++){
        topHand = topHand->next;
    }
    return topHand->top.rank[0];
}

char user_play(struct player* target){
    char rank[2];
    memcpy(rank, get_player_input("Player 1's turn, enter a rank:"), 2);
    if(search(target, rank[0]))
        return rank[0];
    else{
        printf("Error - must have at least one card from rank to play\n");
        user_play(target);
    }
}

void print_user_book(struct player* target){
    printf("%s's Book - ", target->name);
    for(unsigned int i=0; i<strlen(target->book);i++){
        printf("%s ", get_complete_char(target->book[i]));
    }
    printf("\n");
}
int validate_player_input(char input[2]){
    char rank[2];
    memcpy(rank, input, 2);
    ValidCards vcards = valid_cards_default;
    for(unsigned long long i =0; i< sizeof(vcards.ranks)/4;i++){
        if (compare_ranks(rank, vcards.ranks[i]))
            return 1;
    }
    return 0;

}

char* get_player_input(char* prompt){
    static char rank[2];
    printf("%s", prompt);
    // Scan 2 chars from stdin
    scanf("%2s", rank);
    // get char until newline is found to clear stdin
    while ( getchar() != '\n' );
    if(rank[1]=='\n')
        rank[1]='\0';
    if(validate_player_input(rank))
        return rank;
    printf("That rank does not exist, try again\n");
    return get_player_input(prompt);

}

void print_hand_card_of_rank(struct player* target, char rank){
    struct hand* topHand;
    topHand = target->card_list;
    while(topHand){
            printf("%c%c%c ", topHand->top.rank[0], topHand->top.rank[1], topHand->top.suit);
        }
        topHand = topHand->next;
    }
}