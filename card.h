#ifndef CARD_H
#define CARD_H

/*
  Valid suits: C, D, H, and S
  Valid ranks: 2, 3, 4, 5, 6, 7, 8, 9, 10, J, Q, K, A
*/

struct valid_cards
{
    char *ranks[13];
    char suits[4];

}static valid_cards_default={ "2",
                              "3",
                              "4",
                              "5",
                              "6",
                              "7",
                              "8",
                              "9",
                              "10",
                              "J",
                              "Q",
                              "K",
                              "A",
                              'C',
                              'D',
                              'H',
                              'S' };

typedef struct valid_cards ValidCards;

struct card
{
  char suit;
  char rank[2];
  int r;
  int s;
};

/*
  Linked list of cards in hand.
    top: first card in hand
    next: pointer to next card in hand
*/
struct hand
{
  struct card top;
  struct hand* next;
};

int compare_card(struct card* comp1, char comp2);
struct card create_card(char rank[2], char suit);
#endif
