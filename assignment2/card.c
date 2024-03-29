#include <memory.h>
#include "card.h"
int compare_card_rank(struct card* comp1, char comp2){
    if(comp1->rank[0] == comp2)
        return 1;
    else
        return 0;
}

int compare_card(struct card* comp1, struct card* comp2){
    if(strncmp(comp1->rank,comp2->rank,2)==0 && comp1->suit==comp2->suit)
        return 1;
    else
        return 0;
}

int compare_ranks(char rank1[2], char rank2[2]){
    if(strncmp(rank1, rank2, 2)==0)
        return 1;
    return 0;
}

char * get_complete_char(char rank){
    ValidCards vcards = valid_cards_default;
    for(int i=0; i<sizeof(vcards.ranks)/4;i++){
        if(rank == vcards.ranks[i][0]){
            return vcards.ranks[i];
        }
    }

}
struct card create_card(char rank[2], char suit){
    struct card newcard;
    memcpy(newcard.rank, rank,2);
    newcard.suit=suit;
    return newcard;
}
