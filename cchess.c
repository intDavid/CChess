#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

/*  ~~~ BITBOARD REPRESENTATION LAYOUT ~~~
 *  56	57	58	59	60	61	62	63
    48	49	50	51	52	53	54	55
    40	41	42	43	44	45	46	47
    32	33	34	35	36	37	38	39
    24	25	26	27	28	29	30	31
    16	17	18	19	20	21	22	23
    08	09	10	11	12	13	14	15
    00	01	02	03	04	05	06	07
 */

#define BOARD_STRING_SIZE 2004 + 1

// Evaluation values.
#define VALUE_KING 200
#define VALUE_QUEEN 9
#define VALUE_ROOK 5
#define VALUE_BISHOP 3
#define VALUE_KNIGHT 3
#define VALUE_PAWN 1

/* Positional bitwise operations
 * NOTE: @amount ALWAYS needs to be positive
 * --- SHIFTING BY NEGATIVE VALUES IS UNDEFINED BEHAVIOUR IN C. ---
 * Also, it should be checked before calling whether or not @amount is within
 * "legal" bounds (not exceeding the board size), e.g.:
 *      - if @x represents the H8 square (upper right corner) the maximum value
 *      @amount can take for NORTH_EAST is 0, for SOUTH_WEST 7 and for the
 *      other two it is also 0;
 *      - if @x represents the B7 square the maximum value @amount can take for
 *      SOUTH_EAST is 6 and for the other three it is 1.
 */
#define NORTH(x, amount) (x) << (8 * (amount))
#define SOUTH(x, amount) (x) >> (8 * (amount))
#define WEST(x, amount) (x) >> (amount)
#define EAST(x, amount) (x) << (amount)
#define NORTH_EAST(x, amount) (x) << (9 * (amount))
#define NORTH_WEST(x, amount) (x) << (7 * (amount))
#define SOUTH_EAST(x, amount) (x) >> (7 * (amount))
#define SOUTH_WEST(x, amount) (x) >> (9 * (amount))

// Positions on board.
#define WHITE_SQUARES 0x55aa55aa55aa55aa
#define BLACK_SQUARES 0xaa55aa55aa55aa55
#define A_FILE 0x101010101010101
#define B_FILE 0x202020202020202
#define C_FILE 0x404040404040404
#define D_FILE 0x808080808080808
#define E_FILE 0x1010101010101010
#define F_FILE 0x2020202020202020
#define G_FILE 0x4040404040404040
#define H_FILE 0x8080808080808080
#define RANK_1 0xff
#define RANK_2 0xff00
#define RANK_3 0xff0000
#define RANK_4 0xff000000
#define RANK_5 0xff00000000
#define RANK_6 0xff0000000000
#define RANK_7 0xff000000000000
#define RANK_8 0xff00000000000000

// Positions (char array index) on board string.
// There is an horizontal increment of 7 between pieces and a vertical
// increment of 236. For example: the char representing the piece on A8
// will be at @_board[123], the one on B8 at @_board[130] and the one on
// A7 at @_board[123 + 236]
#define S_HORIZONTAL_INC 7
#define S_VERTICAL_INC 236
#define S_A8 123      // upper left corner
#define S_A7 (S_A8 + S_VERTICAL_INC)
#define S_A6 (S_A8 + 2 * S_VERTICAL_INC)
#define S_A3 (S_A8 + 5 * S_VERTICAL_INC)
#define S_A2 (S_A8 + 6 * S_VERTICAL_INC)
#define S_A1 (S_A8 + 7 * S_VERTICAL_INC)
#define S_H1 1824     // lower right corner

// States
#define ST_PLAYING 0xf
#define ST_DRAW 0
#define ST_WHITE_WIN 1
#define ST_WHITE_IN_CHECK 2
#define ST_BLACK_WIN -1
#define ST_BLACK_IN_CHECK -2

#define PLAYER_WHITE 1
#define PLAYER_BLACK -1
uint64_t _wKing;
uint64_t _wQueen;
uint64_t _wRooks;
uint64_t _wBishops;
uint64_t _wKnights;
uint64_t _wPawns;
uint64_t _wPieces;
uint64_t _bKing;
uint64_t _bQueen;
uint64_t _bRooks;
uint64_t _bBishops;
uint64_t _bKnights;
uint64_t _bPawns;
uint64_t _bPieces;

// 16 eligible squares for en passant: the 7th rank and the 2nd rank
uint64_t _enPassant; // could've made it 8bit but it gets promoted anyway

// least significant 2 bits for white, the next two for black
// first king-side, then queen-side (e.g.: for 1101 white can only castle
// kingside but black can castle both ways).
uint8_t _canCastle;

int8_t _player; // 1 = white, -1 = black

// 0xF = playing, 0 = draw/stalemate, -1 black win, 1 = white win
// 2 = white in check, -2 = black in check
int8_t _playingState;

/* The fifty-move rule in chess states that a player can claim a draw if no
 * capture has been made and no pawn has been moved in the last fifty moves
 */
uint8_t _50MoveRuleCount;


char _board[BOARD_STRING_SIZE] =
    "  ########################################################\n"
    "  #     ##     ##     ##     ##     ##     ##     ##     #\n"
    "8 #  O  ##  O  ##  O  ##  O  ##  O  ##  O  ##  O  ##  O  #\n"
    "  #     ##     ##     ##     ##     ##     ##     ##     #\n"
    "  ########################################################\n"
    "  #     ##     ##     ##     ##     ##     ##     ##     #\n"
    "7 #  O  ##  O  ##  O  ##  O  ##  O  ##  O  ##  O  ##  O  #\n"
    "  #     ##     ##     ##     ##     ##     ##     ##     #\n"
    "  ########################################################\n"
    "  #     ##     ##     ##     ##     ##     ##     ##     #\n"
    "6 #  O  ##  O  ##  O  ##  O  ##  O  ##  O  ##  O  ##  O  #\n"
    "  #     ##     ##     ##     ##     ##     ##     ##     #\n"
    "  ########################################################\n"
    "  #     ##     ##     ##     ##     ##     ##     ##     #\n"
    "5 #  O  ##  O  ##  O  ##  O  ##  O  ##  O  ##  O  ##  O  #\n"
    "  #     ##     ##     ##     ##     ##     ##     ##     #\n"
    "  ########################################################\n"
    "  #     ##     ##     ##     ##     ##     ##     ##     #\n"
    "4 #  O  ##  O  ##  O  ##  O  ##  O  ##  O  ##  O  ##  O  #\n"
    "  #     ##     ##     ##     ##     ##     ##     ##     #\n"
    "  ########################################################\n"
    "  #     ##     ##     ##     ##     ##     ##     ##     #\n"
    "3 #  O  ##  O  ##  O  ##  O  ##  O  ##  O  ##  O  ##  O  #\n"
    "  #     ##     ##     ##     ##     ##     ##     ##     #\n"
    "  ########################################################\n"
    "  #     ##     ##     ##     ##     ##     ##     ##     #\n"
    "2 #  O  ##  O  ##  O  ##  O  ##  O  ##  O  ##  O  ##  O  #\n"
    "  #     ##     ##     ##     ##     ##     ##     ##     #\n"
    "  ########################################################\n"
    "  #     ##     ##     ##     ##     ##     ##     ##     #\n"
    "1 #  O  ##  O  ##  O  ##  O  ##  O  ##  O  ##  O  ##  O  #\n"
    "  #     ##     ##     ##     ##     ##     ##     ##     #\n"
    "  ########################################################\n"
    "     A      B      C      D      E      F      G      H\n\n";

void clearBitPos(uint64_t *var, uint64_t x) {
    *var &= (~1u << x);
}

void setBitPos(uint64_t *var, uint64_t x) {
    *var |= (1 << x);
}

// Returns either 0 or a value greater than 0.
uint64_t getBitPos(const uint64_t *var, uint64_t x) {
    return *var & (1 << x);
}

/* For versatility's sake, the board will always look the same but the way
 * pieces and empty squares are represented can be changed. This function works
 * for the random variant too.
 */
void init_board_string() {
#define A8 (1ull << (8 * 7))
#define A2 (1ull << 8)
#define S_OPEN_SQUARE ' ' // must be a single char

    uint64_t *ptrBlackPieces[6] = {
        &_bPawns, &_bRooks, &_bKnights, &_bBishops, &_bQueen, &_bKing
    };
    uint64_t *ptrWhitePieces[6] = {
        &_wPawns, &_wRooks, &_wKnights, &_wBishops, &_wQueen, &_wKing
    };

    enum Piece {
        Pawn = 0, Rook, Knight, Bishop, Queen, King
    };

    char c;
    unsigned i, j, k;
    uint64_t square = A8;
    // black pieces
    for (i = S_A8; i <= S_A7; i = i + S_VERTICAL_INC) {
        for (j = i; j <= i + S_HORIZONTAL_INC * 7; j = j + S_HORIZONTAL_INC) {
            c = S_OPEN_SQUARE;
            // determine what black piece resides here
            for (k = 0; k < 6; ++k) {
                if (*ptrBlackPieces[k] & square) {
                    switch (k) {
                        case Pawn :
                            c = 'p';
                            break;
                        case Rook :
                            c = 'r';
                            break;
                        case Knight:
                            c = 'n';
                            break;
                        case Bishop :
                            c = 'b';
                            break;
                        case Queen :
                            c = 'q';
                            break;
                        case King :
                            c = 'k';
                            break;
                        default:
                            c = '?';
                    }
                }
            }
            _board[j] = c;
            square = EAST(square, 1);
        }
        square = SOUTH(A8, 1); // A7
    }

    // open squares
    for (i = S_A6; i <= S_A3; i = i + S_VERTICAL_INC) {
        for (j = i; j <= i + S_HORIZONTAL_INC * 7; j = j + S_HORIZONTAL_INC) {
            _board[j] = S_OPEN_SQUARE;
        }
    }

    // white pieces
    square = A2;
    for (i = S_A2; i <= S_A1; i = i + S_VERTICAL_INC) {
        for (j = i; j <= i + S_HORIZONTAL_INC * 7; j = j + S_HORIZONTAL_INC) {
            c = S_OPEN_SQUARE;
            for (k = 0; k < 6; ++k) {
                if (*ptrWhitePieces[k] & square) {
                    switch (k) {
                        case Pawn :
                            c = 'P';
                            break;
                        case Rook :
                            c = 'R';
                            break;
                        case Knight:
                            c = 'N';
                            break;
                        case Bishop :
                            c = 'B';
                            break;
                        case Queen :
                            c = 'Q';
                            break;
                        case King :
                            c = 'K';
                            break;
                        default:
                            c = '?';
                    }
                }
            }
            _board[j] = c;
            square = EAST(square, 1);
        }
        square = SOUTH(A2, 1); // A1
    }
}

void init_board() {
    _wKing = RANK_1 & E_FILE;
    _wQueen = RANK_1 & D_FILE;
    _wRooks = (RANK_1 & A_FILE) | (RANK_1 & H_FILE);
    _wBishops = (RANK_1 & C_FILE) | (RANK_1 & F_FILE);
    _wKnights = (RANK_1 & B_FILE) | (RANK_1 & G_FILE);
    _wPawns = RANK_2;

    _bKing = RANK_8 & E_FILE;
    _bQueen = RANK_8 & D_FILE;
    _bRooks = (RANK_8 & A_FILE) | (RANK_8 & H_FILE);
    _bBishops = (RANK_8 & C_FILE) | (RANK_8 & F_FILE);
    _bKnights = (RANK_8 & B_FILE) | (RANK_8 & G_FILE);
    _bPawns = RANK_7;

    _wPieces = _wKing | _wQueen | _wRooks | _wBishops | _wKnights | _wPawns;
    _bPieces = _bKing | _bQueen | _bRooks | _bBishops | _bKnights | _bPawns;
}

// Generate source squares of pawns able to push
// TODO (latest): Optimize (check 1 square advance and then the second one (if starting on r2))
void gen_PawnMoves(uint64_t *board) {
    *board = 0;
    if (_player == PLAYER_WHITE) {
        if (RANK_2 & _wPawns) {
            // those pawns can move two squares
            // TODO: check for white pieces in the way
            *board = *board |
                (NORTH((RANK_2 & _wPawns), 2) &
                ~((RANK_4 & _bPieces) | (NORTH((RANK_3 & _bPieces), 1)) |
                (RANK_4 & _wPieces) | (NORTH((RANK_3 & _wPieces), 1))));
        }
        // 1 square advance for every pawn
    }
}

// generates the entire bitboard for available moves every time it's called
// will optimize later; TODO: optimize gen_move()
void gen_move(uint64_t *moveBoard, unsigned piece) {
    *moveBoard = 0;
    enum Pieces {
        Pawn = 0, Knight, Bishop, Rook, Queen, King
    };
    switch (piece) {
        case Pawn:

            break;
        case Knight:

            break;
        case Bishop:

            break;
        case Rook:

            break;
        case Queen:

            break;
        case King:

            break;
        default:
            *moveBoard = 0; // redundant?
            break;
    }
}

// TODO: rename @_playingState to @_state
void play() {
    _playingState = ST_PLAYING; // new game

    enum Pieces {
        Pawn = 0, Knight, Bishop, Rook, Queen, King
    };
    uint64_t wMoveBoards[6];
    uint64_t bMoveBoards[6];

    while (_playingState == ST_PLAYING) {
        // generate moves for side, see source piece, pick one move randomly
        if (_player == PLAYER_WHITE) {
            // white moves

        }
        if (_player == PLAYER_BLACK) {
            // black moves
        }
        // check for a change of state (win or check or whatever)
    }
}

void print_board() {
    printf("%s", _board);
}

int main() {
    srand((unsigned int) time(NULL));
    init_board();
    init_board_string();
    print_board();

    return 0;
}

/* TODO: Add states (castling rights, en passant, 50-move rule)
 * TODO: Add play(), random variant for init_board()
 */
