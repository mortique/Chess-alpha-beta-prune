
// Shakkimoottori.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
// Todo 1. add dynamic search depth if needed in cases where only few moves are searched within searchlimit
// 2. add extra depth for capturing last moves if needed
// 3. refine and incorporate refined evaluation (and add all tied scores instead of n)
// 4. Add castling, promotions, en passant
// 29.6 fixed bug in isCheck and moveBack where king position doesn't correctly update in reverse moves
// GUI was generated with AI

// ######
// Requirements for running GUI-version(compiled in Ubuntu):
// - font (now hardcoded dejavu.ttf) in the same folder to run
// - SFML library for visual gui
// - Compiling in command line, visual studio won't run it with SFML
// ######

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <limits>
#include <algorithm>
#include <climits>
#include <array>
#include <cstdint>

// Constants
constexpr int MAX_MOVES = 256;
const int TILE_SIZE = 80;
const int BOARD_SIZE = 8;

using namespace std;

const int KING = 5;
const int QUEEN = 6;
const int ROOK = 2;
const int BISHOP = 4;
const int KNIGHT = 3;
const int PAWN = 1;

const int WHITE = 1;
const int BLACK = -1;

const int MATE = 999;

const int CHECKMATE = 3;
const int STALEMATE = 2;
const int CHECK = 1;

int nodesSearched = 0;

// ######## Global parameters

int maxDepth = 5;
int searchLimit = 500000;

// ########

struct Move {
    int8_t player;
    uint8_t x0, y0;
    uint8_t x, y;
    uint8_t captured_value = 0;
    int evaluation;
    bool givesCheck = false;

    // Return forward movement
    int8_t getProgress() const{
        if(player == 1)
            return int8_t(y - y0);
        else
            return int8_t(y0 - y);
    }

    Move() = default;

    Move(const Move&) = default;
    Move& operator=(const Move&) = default;

    bool operator==(const Move& other) const {
        constexpr float epsilon = 1e-5;
        return player == other.player &&
               x0 == other.x0 && y0 == other.y0 &&
               x == other.x && y == other.y &&
               captured_value == other.captured_value &&
               givesCheck == other.givesCheck &&
               std::fabs(evaluation - other.evaluation) < epsilon;
    }

Move(int8_t player_, uint8_t x0_, uint8_t y0_, uint8_t x_, uint8_t y_, uint8_t captured_value_, bool givesCheck_ = false)
    : player(player_), x0(x0_), y0(y0_), x(x_), y(y_), captured_value(captured_value_), givesCheck(givesCheck_) {}

};

struct Move_h { // move history entry, containing captured pieces
    Move move;
    int8_t captured_piece;      // retain + - sign here

    Move_h(const Move& m, int8_t captured) {
        move = m;
        captured_piece = captured;
    }
};

struct Evaluation {
    Move move, opponentBestMove;
    vector<Move> opponentMoveList;
    int score = 0;
    int8_t material = 0;
    int8_t mate = 0;    // -1 player in mate, 1 player mated opponent

    Evaluation() = default;

    Evaluation(const Move& m, int score_, int8_t material_){
        move = m;
        score = score_;
        material = material_;
        mate = 0;
    }

    Evaluation(int8_t mate_){
        mate = mate_;
    }


};

struct Node {
    int value;
    int evaluation;
    int refinedEval;
    Move move;
    int depth;
    std::vector<Node*> children;
    Node* parent = nullptr;
    Node* lastAnalyzedNode = nullptr;
    bool terminatedSearch = false;
    vector<Node*> bestReplies;
    bool hasCapture = false;
    bool chainHasCaptures = false;
    int ownMaterialUnderThreat = 0;
    int ThreateningMaterial = 0;


    Node() = default;

    Node(int val) : value(val) {}

    Node(const Node&) = default;
    Node& operator=(const Node&) = default;

};

struct EvalResult {
    int evaluation;
    Node* node;
};

class Board {
  public:              
     std::array<std::array<int8_t, BOARD_SIZE>, BOARD_SIZE> board{};
    std::array<uint8_t, 2> kx{}, ky{};
    int8_t turn = 1;
    std::vector<Move_h> history;
    int kingToCheck_x, kingToCheck_y;

    Board() {
        int8_t init[8][8] = {
            {2, 3, 4, 6, 5, 4, 3, 2},
            {1, 1, 1, 1, 1, 1, 1, 1},
            {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0},
            {-1, -1, -1, -1, -1, -1, -1, -1},
            {-2, -3, -4, -6, -5, -4, -3, -2},
        };

        for (int y = 0; y < BOARD_SIZE; ++y)
            for (int x = 0; x < BOARD_SIZE; ++x)
                board[y][x] = init[y][x];

        findKings();
    }

void findKings() {
    // Reset positions to invalid initially
    kx[0] = ky[0] = 8;
    kx[1] = ky[1] = 8;

    for (uint8_t y = 0; y < 8; ++y) {
        for (uint8_t x = 0; x < 8; ++x) {
            int piece = board[y][x];
            if (piece == KING) {
                kx[1] = x;
                ky[1] = y;
            } else if (piece == -KING) {
                kx[0] = x;
                ky[0] = y;
            }
        }
    }
}

    void reset() {
        while (!history.empty()) moveBack();
    }

    void updateKingPosition(const Move& move, bool reverseMove = false) {
        int val;
        if(!reverseMove){
            val = getValue(move.x0, move.y0);
            if (val == KING) { kx[1] = move.x; ky[1] = move.y; }
            else if (val == -KING) { kx[0] = move.x; ky[0] = move.y; }
        }
        else{ 
            val = getValue(move.x, move.y);
            if (val == KING) { kx[1] = move.x0; ky[1] = move.y0; }
            else if (val == -KING) { kx[0] = move.x0; ky[0] = move.y0; }
        }
    }

    uint8_t move(const Move& move, bool reverseMove = false) {
        updateKingPosition(move);
        int8_t captured = board[move.y][move.x];
        if (!reverseMove) history.push_back({ move, captured });
        board[move.y][move.x] = board[move.y0][move.x0];
        board[move.y0][move.x0] = 0;
        turn *= -1;
        return getPieceValue(captured);
    }

    void moveBack() {
        if (!history.empty()) {
            auto& last = history.back();
            updateKingPosition(last.move, true); // True for reverse move (king is at x,y)
            board[last.move.y0][last.move.x0] = board[last.move.y][last.move.x];
            board[last.move.y][last.move.x] = last.captured_piece;
            turn *= -1;
            history.pop_back();
        }
    }

    inline uint8_t getPieceValue(int piece) const {
        switch (std::abs(piece)) {
            case PAWN: return 1;
            case KNIGHT: case BISHOP: return 3;
            case ROOK: return 5;
            case QUEEN: return 9;
            default: return 0;
        }
    }

    inline int getValue(int x, int y) const {
        return board[y][x];
    }

    string getPieceANSICode(int piece, int bgColor = 0) const {
        string colorToAdd = "";
        if(bgColor != 0)
            colorToAdd = "\033[37;44m";
        switch (piece * -1){        // colors in terminal seem visibly opposite -> *-1
            case 0:
                return " ";
            case PAWN:              // white pieces
                return "\u2659";
            case ROOK:
                return "\u2656";
            case KNIGHT:
                return "\u2658";
            case BISHOP:
                return "\u2657";
            case KING:
                return "\u2654";
            case QUEEN:
                return "\u2655";
            case -1:                // black pieces
                return "\u265F";
            case -2:
                return "\u265C";
            case -3:
                return "\u265E";
            case -4:
                return "\u265D";
            case -5:
                return "\u265A";
            case -6:
                return "\u265B";
            default:
                return "x";

        }
    }

    // Print board, highlight square
    void printBoard(int x = -1, int y = -1) const {
        for (int i = 7; i >= 0; --i){
            cout << i;
            for (int j = 0; j < 8; ++j){
                // Color latest move
                if(i == y && j == x)
                    cout << "\033[37;44m" << getPieceANSICode(board[i][j], 1) << "\033[49m" << " ";
                else
                    cout << getPieceANSICode(board[i][j], 1) << " ";
            }
            cout << std::endl;
        }
        cout << " 0 1 2 3 4 5 6 7" << std::endl;
    }
};


// ######### GUI (AI generated)
// Helper to get board square from mouse
#include <SFML/Graphics.hpp>
sf::Vector2i getBoardPos(sf::Vector2i pixelPos) {
    return sf::Vector2i(pixelPos.x / TILE_SIZE, pixelPos.y / TILE_SIZE);
}

sf::Font font;

sf::Text makePieceText(int pieceValue, int x, int y) {
    sf::Text text;
    text.setFont(font);
    text.setCharacterSize(48);
    text.setFillColor(pieceValue > 0 ? sf::Color::White : sf::Color::Black);
    text.setPosition(x * TILE_SIZE + 20, y * TILE_SIZE + 10);

    char32_t c = U' ';
    switch (std::abs(pieceValue)) {
        case 1: c = (pieceValue > 0) ? U'\u2659' : U'\u265F'; break;  // Pawn
        case 2: c = (pieceValue > 0) ? U'\u2656' : U'\u265C'; break;  // Rook
        case 3: c = (pieceValue > 0) ? U'\u2658' : U'\u265E'; break;  // Knight
        case 4: c = (pieceValue > 0) ? U'\u2657' : U'\u265D'; break;  // Bishop
        case 5: c = (pieceValue > 0) ? U'\u2654' : U'\u265A'; break;  // King
        case 6: c = (pieceValue > 0) ? U'\u2655' : U'\u265B'; break;  // Queen
    }
    text.setString(c);
    return text;
}

void drawBoard(sf::RenderWindow &window, Board &board) {
    window.clear();
    
    for (int y = 0; y < BOARD_SIZE; ++y) {
        for (int x = 0; x < BOARD_SIZE; ++x) {
            sf::RectangleShape square(sf::Vector2f(TILE_SIZE, TILE_SIZE));
            square.setPosition(x * TILE_SIZE, y * TILE_SIZE);
            if ((x + y) % 2 == 0)
                square.setFillColor(sf::Color(240, 217, 181)); // light
            else
                square.setFillColor(sf::Color(181, 136, 99)); // dark
            window.draw(square);

            int pieceValue = board.getValue(x, y);
            if (pieceValue != 0) {
                auto text = makePieceText(pieceValue, x, y);
                window.draw(text);
            }
        }
    }

    window.display();
}


// Class handling moves for piece
class PieceMoves {
    public:   

std::vector<Move> getPossibleMovesforPiece(uint8_t x0, uint8_t y0, Board& board){
    std::vector<Move> pieceMoveList;
    int n = 0; 
    switch (abs(board.getValue(x0, y0))) {
        case ROOK: {
            int8_t dx[] = { 1, -1, 0, 0 };
            int8_t dy[] = { 0, 0, 1, -1 };
            for (int dir = 0; dir < 4; dir++) {
                uint8_t x = x0 + dx[dir];
                uint8_t y = y0 + dy[dir];
                while (x < 8 && y < 8) {
                    if ((board.getValue(x, y) * board.turn) <= 0) {
                        pieceMoveList.emplace_back(board.turn, x0, y0, x, y, board.getPieceValue(board.board[y][x]), false);
                        n++;
                    } else break;
                    if (board.getValue(x, y) != 0) break;
                    x += dx[dir];
                    y += dy[dir];
                }
            }
            break;
        }
        case BISHOP: {
            int8_t dx[] = { 1, 1, -1, -1 };
            int8_t dy[] = { -1, 1, -1, 1 };
            for (int dir = 0; dir < 4; dir++) {
                uint8_t x = x0 + dx[dir];
                uint8_t y = y0 + dy[dir];
                while (x < 8 && y < 8) {
                    if ((board.getValue(x, y) * board.turn) <= 0) {
                        pieceMoveList.emplace_back(board.turn, x0, y0, x, y, board.getPieceValue(board.board[y][x]), false);
                        n++;
                    } else break;
                    if (board.getValue(x, y) != 0) break;
                    x += dx[dir];
                    y += dy[dir];
                }
            }
            break;
        }
        case KNIGHT: {
            int dx[] = { 2, 1, -1, -2, -2, -1, 1, 2 };
            int dy[] = { 1, 2, 2, 1, -1, -2, -2, -1 };
            for (int i = 0; i < 8; i++) {
                uint8_t x = x0 + dx[i];
                uint8_t y = y0 + dy[i];
                if (x < 8 && y < 8 && (board.getValue(x, y) * board.turn) <= 0) {
                    pieceMoveList.emplace_back(board.turn, x0, y0, x, y, board.getPieceValue(board.board[y][x]), false);
                    n++;
                }
            }
            break;
        }
        case QUEEN: {
            int8_t dx[] = { 1, -1, 0, 0, 1, -1, 1, -1 };
            int8_t dy[] = { 0, 0, 1, -1, 1, 1, -1, -1 };
            for (int dir = 0; dir < 8; dir++) {
                uint8_t x = x0 + dx[dir];
                uint8_t y = y0 + dy[dir];
                while (x < 8 && y < 8) {
                    if ((board.getValue(x, y) * board.turn) <= 0) {
                        pieceMoveList.emplace_back(board.turn, x0, y0, x, y, board.getPieceValue(board.board[y][x]), false);
                        n++;
                    } else break;
                    if (board.getValue(x, y) != 0) break;
                    x += dx[dir];
                    y += dy[dir];
                }
            }
            break;
        }
        case PAWN: {
            int8_t dir = board.turn; // +1 for white, -1 for black
            uint8_t x = x0;
            uint8_t y = y0 + dir;

            // forward 1 square
            if (y < 8 && board.getValue(x, y) == 0) {
                pieceMoveList.emplace_back(board.turn, x0, y0, x, y, 0, false);
                n++;
                int startRow = (board.turn == 1) ? 1 : 6;
                int y2 = y0 + 2 * dir;
                if (y0 == startRow && y2 < 8 && board.getValue(x, y2) == 0) {
                    pieceMoveList.emplace_back(board.turn, x0, y0, x, y2, 0, false);
                    n++;
                }
            }

            // capture diagonally left
            if (x0 > 0 && y < 8 && (board.getValue(x0 - 1, y) * board.turn) < 0) {
                pieceMoveList.emplace_back(board.turn, x0, y0, x0 - 1, y, board.getPieceValue(board.board[y][x0 - 1]), false);
                n++;
            }
            // capture diagonally right
            if (x0 < 7 && y < 8 && (board.getValue(x0 + 1, y) * board.turn) < 0) {
                pieceMoveList.emplace_back(board.turn, x0, y0, x0 + 1, y, board.getPieceValue(board.board[y][x0 + 1]), false);
                n++;
            }
            break;
        }
        case KING: {
            int dx[] = { 1, 1, 0, -1, -1, -1, 0, 1 };
            int dy[] = { 0, 1, 1, 1, 0, -1, -1, -1 };
            for (int i = 0; i < 8; i++) {
                uint8_t x = x0 + dx[i];
                uint8_t y = y0 + dy[i];
                if (x < 8 && y < 8 && (board.getValue(x, y) * board.turn) <= 0) {
                    Move m;
                    m.x0 = x0; m.y0 = y0; m.x = x; m.y = y; m.player = board.turn;
                    board.move(m);
                    if (!isBoardInCheck(board)) {
                        pieceMoveList.emplace_back(board.turn, x0, y0, x, y, board.getPieceValue(board.board[y][x]), false);
                        n++;
                    }
                    board.moveBack();
                }
            }
            break;
        }
    }
    return pieceMoveList;
}

// Simulate move to check for checks (for player == is move legal)
bool isCheck(uint8_t x0, uint8_t y0, uint8_t x, uint8_t y, Board& board, bool isKingMove = false) {
        Move move;

        move.player = board.turn;
        move.x0 = x0;
        move.y0 = y0;
        move.x = x;
        move.y = y;

        if(isKingMove){         // don't want to find kings for every potential move, hence tracking king position
            board.kingToCheck_x = x;
            board.kingToCheck_y = y;
        }
        else {
            board.kingToCheck_x = board.kx[(board.turn == 1) ? 1 : 0];
            board.kingToCheck_y = board.ky[(board.turn == 1) ? 1 : 0];
        }
        
        board.move(move);
        
        if (isBoardInCheck(board)) {
            board.moveBack();
            return true;
        }
        else{
            board.moveBack();
            return false;
        } 
    }

    // See if given board is in check
    bool isBoardInCheck(Board& board, int8_t color = -9) { 
        // No parameter given (color=9) -> coming from isCheck function, get custom king to check values for simulated move
        if(color == -9){
            if(board.getValue(board.kingToCheck_x, board.kingToCheck_y) == 5)
                color = 1;
            else if(board.getValue(board.kingToCheck_x, board.kingToCheck_y) == -5)
                color = -1;
            else {
                // board.printBoard();
                // cout << "error in king location";
            }
        }
        // Coming from evaluation to see if move makes a check (color value doesn't really matter here)
        else {
            board.kingToCheck_x = board.kx[(board.turn == 1) ? 1 : 0];
            board.kingToCheck_y = board.ky[(board.turn == 1) ? 1 : 0];
            color = board.turn;
        }
        
        int kingX = board.kingToCheck_x;
        int kingY = board.kingToCheck_y;

        // Direction vectors
        int straightDirs[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
        int diagonalDirs[4][2] = { {1,1}, {-1,1}, {1,-1}, {-1,-1} };
        int knightMoves[8][2] = { {1,2}, {2,1}, {2,-1}, {1,-2}, {-1,-2}, {-2,-1}, {-2,1}, {-1,2} };
        int kingMoves[8][2] = { {1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {-1,1}, {1,-1}, {-1,-1} };

        auto onBoard = [](int x, int y) {
            return x >= 0 && x < 8 && y >= 0 && y < 8;
        };

        // Check for rook/queen threats
        for (auto& dir : straightDirs) {
            int x = kingX + dir[0];
            int y = kingY + dir[1];
            while (onBoard(x, y)) {
                int piece = board.getValue(x, y);
                if (piece == 0) {
                    x += dir[0];
                    y += dir[1];
                    continue;
                }
                if ((piece * color) < 0 && (abs(piece) == ROOK || abs(piece) == QUEEN))
                    return true;
                break;
            }
        }

        // Check for bishop/queen threats
        for (auto& dir : diagonalDirs) {
            int x = kingX + dir[0];
            int y = kingY + dir[1];
            while (onBoard(x, y)) {
                int piece = board.getValue(x, y);
                if (piece == 0) {
                    x += dir[0];
                    y += dir[1];
                    continue;
                }
                if ((piece * color) < 0 && (abs(piece) == BISHOP || abs(piece) == QUEEN))
                    return true;
                break;
            }
        }

        // Check for knight threats
        for (auto& move : knightMoves) {
            int x = kingX + move[0];
            int y = kingY + move[1];
            if (onBoard(x, y)) {
                int piece = board.getValue(x, y);
                if ((piece * color) < 0 && abs(piece) == KNIGHT)
                    return true;
            }
        }

        // Check for pawn threats
        int dir = color;  // white: +1 (moves up), black: -1 (moves down)
        for (int dx : {-1, 1}) {
            int x = kingX + dx;
            int y = kingY + dir;
            if (onBoard(x, y)) {
                int piece = board.getValue(x, y);
                if ((piece * color) < 0 && abs(piece) == PAWN)
                    return true;
            }
        }

        // adjacent king (invalid king move)
        for (auto& move : kingMoves) {
            int x = kingX + move[0];
            int y = kingY + move[1];
            if (onBoard(x, y)) {
                int piece = board.getValue(x, y);
                if ((piece * color) < 0 && abs(piece) == KING)
                    return true;
            }
        }

        return false;
    }

};


// Find all moves for the player
vector<Move> findPlayerMoves(Board& board, bool checkIfAny = false) {
    PieceMoves pieceMoves;
    vector<Move> playerMoveList;
    playerMoveList.reserve(60);  // typical number of legal moves is under 60

    int turn = board.turn;

    for (uint8_t y = 0; y < 8; ++y) {
        for (uint8_t x = 0; x < 8; ++x) {
            int piece = board.getValue(x, y);
            if (piece * turn > 0) {
                auto moves = pieceMoves.getPossibleMovesforPiece(x, y, board);
                playerMoveList.insert(playerMoveList.end(), moves.begin(), moves.end());

                // Return to see if any legal moves exist
                if(checkIfAny && moves.size() > 0){
                    for(Move& m: moves)
                        if(!pieceMoves.isCheck(m.x0, m.y0, m.x, m.y, board))
                            return moves;
                }
            }
        }
    }

    // Sort captures first
    std::sort(playerMoveList.begin(), playerMoveList.end(), [](const Move& a, const Move& b) {
        if ((a.captured_value > 0) != (b.captured_value > 0))
            return a.captured_value > 0;
        return a.captured_value > b.captured_value;
        // Prioritizing checks; results in less aggressive play
        // if (a.givesCheck != b.givesCheck) return a.givesCheck;
    });

    return playerMoveList;
}
int findPlayerPieces(const Board& board, bool isWhite) {
    int total = 0;
    for (uint8_t y = 0; y < 8; ++y) {
        for (uint8_t x = 0; x < 8; ++x) {
            int piece = board.getValue(x, y);
            if ((isWhite && piece > 0) || (!isWhite && piece < 0)) {
                total += board.getPieceValue(piece);
            }
        }
    }
    return total;
}

int evaluateBoard(const Board& board){
    int whiteValue = findPlayerPieces(board, true);
    int blackValue = findPlayerPieces(board, false);
    return whiteValue - blackValue; // positive = White is better
}

void sortMoveList(std::vector<Move>& moveList) {
    if (moveList.empty()) return;

    std::sort(moveList.begin(), moveList.end(), [](const Move& a, const Move& b) {
        return a.captured_value > b.captured_value;
    });
}

// Returns state of board
int boardState(Board& board){
    PieceMoves PieceMoves;
    vector<Move> moves = findPlayerMoves(board, true);
    if(PieceMoves.isBoardInCheck(board, board.turn)){
        if(moves.size() == 0){
            return CHECKMATE;
        }
        else {
            return CHECK;
        }
    }
    else if(moves.size() == 0)
        return STALEMATE;
    return 0;
}

// Returns whether board is playable and valid
bool isBoardValid(const Board& board){
    Board copy = board;
    vector<Move> moves;

    int state = boardState(copy);
    // Check for stale or checkmates
    if(state == 3){
        //cout << "Checkmate!" << endl;
        return false;
    }
    else if(state == 1){
        //cout << "Check!" << endl;
        return true;
    }
    else if(state == 2){
        //cout << "Stalemate!" << endl;
        return false;
    }

    moves = findPlayerMoves(copy);

    // Check if player can capture king
    for(Move m: moves){
        if(abs(copy.getValue(m.x, m.y)) == 5){
            //cout << "King is capturable!" << endl;
            return false;
        }
    }

    return true;
}

void printMove(const Move move, const Board& board){
    cout << board.getPieceANSICode(static_cast<int>(board.getValue(move.x0, move.y0))) << " From (" << +move.x0 << "," << +move.y0 << ") to (" << +move.x << "," << +move.y << ")" <<  std::endl;
}

void printMoves(Node* node, const Board& board){
    Node currentNode = *node;
    while(currentNode.parent != nullptr){
        cout << board.getPieceANSICode(static_cast<int>(board.getValue(currentNode.move.x0, currentNode.move.y0))) << " From (" << +currentNode.move.x0 << "," << +currentNode.move.y0 << ") to (" << +currentNode.move.x << "," << +currentNode.move.y << ")" <<  std::endl;
        currentNode = *currentNode.parent;
    }
    cout << "Value " << node->evaluation << endl;
    cout << "#########" << endl;
}


void deleteTree(Node* node) {
    if (!node) return;
    for (Node* child : node->children) {
        deleteTree(child);
    }
    delete node;
}

void keepTopN(std::vector<Node*>& nodeList, int keepN, bool isMaximizingPlayer) {
    if (nodeList.empty() || keepN <= 0) return;

    int total = nodeList.size();
    int toRemove = total - keepN;

    if (toRemove >= total) {
        nodeList.clear();
        return;
    }

    std::sort(nodeList.begin(), nodeList.end(), [isMaximizingPlayer](const Node* a, const Node* b) {
        return isMaximizingPlayer ? (a->refinedEval > b->refinedEval)
                                  : (a->refinedEval < b->refinedEval);
    });

    nodeList.resize(keepN); // Keep only the top N
}

// Return all moves from nodechain
vector<Move> getMoves(Node* node) {
    vector<Move> moves;
    Node* currentNode = node;
    // Get all but nominal root node
    while (currentNode->depth >= 0) {
        moves.push_back(currentNode->move);
        currentNode = currentNode->parent;
    }
    std::reverse(moves.begin(), moves.end());
    return moves;
}

// Play all moves from nodechain
void PlayNodeMoves(Board& board, Node* node, bool playMoves = false) {
    vector<Move> moves = getMoves(node);
    board.printBoard();
    for(Move& m: moves){
        board.move(m);
        // Play silently unless for debugging
        if(!playMoves){
            cout << "##############" << endl;
            board.printBoard(m.x, m.y);
        }
    }
    // Resume board state after printing moves
    if(!playMoves)
        for(int i=0; i < moves.size(); i++) board.moveBack();
}

// Return rootnode from node
Node* getRoot(Node* node) {
    Node* currentNode = node;
    Node* lastRealNode = nullptr;

    while (currentNode->parent != nullptr) {
        lastRealNode = currentNode;
        currentNode = currentNode->parent;
    }
    return lastRealNode;
}

std::string moveToStr(const Move& move) {
    auto coordToStr = [](int x, int y) -> std::string {
        char file = 'a' + x;       // x: 0 → 'a', ..., 7 → 'h'
        char rank = '1' + y;       // y: 0 → '1', ..., 7 → '8'
        return std::string{file, rank};
    };

    return coordToStr(move.x0, move.y0) + coordToStr(move.x, move.y);
}

bool isMaximizingAtDepth(int rootTurn, int depth) {
    bool rootIsMax = (rootTurn == 1); // true for White
    return (depth % 2 == 0) ? rootIsMax : !rootIsMax;
}

// Get material threatened for opponent or player
int materialUnderThreat(Board& board, bool threateningMaterial = false){
    // Check for opponent or player threats
    if(!threateningMaterial) board.turn *= -1;

    int capturableValue = 0;
    vector<Move> opponentMoves = findPlayerMoves(board);

    for(Move m : opponentMoves)
        m.captured_value += capturableValue;

    if(!threateningMaterial) board.turn *= -1;
    return capturableValue;
}

Move getBestMove(Board board, int maxDepth, int searchLimit);

EvalResult alphaBeta(Board& board, int depth, int alpha, int beta, Node* parent = nullptr, int searchLimit = 200000, int currentDepth = 0) {
    EvalResult evalResult;
    Node* bestNode = nullptr;

    std::vector<Move> moves = findPlayerMoves(board);

    // ###### Break conditions
    int state = 0;
    // Check or stalemate
    if(moves.size() == 0)
        state = boardState(board);

    if (depth <= 0 || state > 1 || nodesSearched > searchLimit) {
        evalResult.evaluation = evaluateBoard(board);
        evalResult.node = parent;
        return evalResult;
    }

    bool isWhite = board.turn == 1;
    int bestEval = isWhite ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();

    for (size_t i = 0; i < moves.size(); ++i) {
        Move move = moves[i];

        // Make the move
        board.move(move);

        if(boardState(board) > 1){
            board.moveBack();
            continue;
        }

        // Node tracking (currently legacy)
        size_t childVal = static_cast<size_t>((parent ? parent->value : 1) * 100 + i);
        Node* child = new Node(childVal);
        child->move = move;
        child->parent = parent;
        child->depth = currentDepth + 1;

        if(parent)
            parent->children.push_back(child);

        // Check for check&stalemate
        if(child->move.givesCheck)
            if(boardState(board) == CHECKMATE){
                board.moveBack();
                break;
        }

        nodesSearched++;

        evalResult = alphaBeta(board, depth - 1, alpha, beta, child, searchLimit, currentDepth + 1);
        int eval = evalResult.evaluation;

        if (isWhite) {
            if(eval > bestEval){
                bestNode = evalResult.node;
            }
            bestEval = std::max(bestEval, eval);
            alpha = std::max(alpha, eval);
        } else {
            if(eval < bestEval){
                bestNode = evalResult.node;
            }
            bestEval = std::min(bestEval, eval);
            beta = std::min(beta, eval);
        }

        board.moveBack();
        
        // Prune
        if (beta <= alpha) {
            child->terminatedSearch = true;
            break;
        }
    }
    
    // Return evaluation and bestnode
    evalResult.evaluation = bestEval;
    evalResult.node = bestNode ? bestNode : parent; // Fallback to parent if nothing found
    return evalResult;
}

Move getBestMove(Board board, int maxDepth, int searchLimit) {
    nodesSearched = 0;
    EvalResult evalResult;
    Node* root = new Node(0); // Root node for tracking
    root->depth = -1;
    
    constexpr size_t N = 5; // Keep top N moves
    
    // Find all moves
    std::vector<Move> moves = findPlayerMoves(board);

    bool isMaximizing = (board.turn == 1);

    int bestEval = isMaximizing ? std::numeric_limits<int>::min()
                                : std::numeric_limits<int>::max();
    Move bestMove;
    Node* bestNode = nullptr;

    // Track how many initial moves are analyzed with current searchlimit
    int initMovesSearched = 0;

    std::vector<std::pair<int, Node>> topMoves;

    for (int i = 0; i < moves.size(); ++i) {
        initMovesSearched++;
        Move move = moves[i];

        Board newBoard = board;
        newBoard.move(move);

        if(!isBoardValid(newBoard)){
            continue;
        }

        size_t childVal = root->value * 100 + i + 1;
        Node* child = new Node(childVal);
        child->move = move;
        child->parent = root;
        child->depth = 0;

        // Check for mate in 1
        if(move.givesCheck)
            if(boardState(newBoard) == CHECKMATE){
                bestMove = move;
                return bestMove;
            }

        root->children.push_back(child);

        evalResult = alphaBeta(newBoard, maxDepth - 1, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), child, searchLimit, 0);

        // Store pointer to last node in analysis
        child->lastAnalyzedNode = evalResult.node;
        
        // Store and sort top moves
        // Evaluation comes from deeper, move is the first move
        topMoves.emplace_back(evalResult.evaluation, *child);
        std::sort(topMoves.begin(), topMoves.end(), [isMaximizing](const std::pair<int, Node>& a, const std::pair<int, Node>& b) {
            return isMaximizing ? a.first > b.first : a.first < b.first;
        });
        if (topMoves.size() > N)
            topMoves.pop_back();

        if(nodesSearched > searchLimit)
            break;
    }
    
    int nMoves = moves.size();
    cout << "Nodes searched: " << nodesSearched << " Init moves searched: " << initMovesSearched << "/" << nMoves << " Best evaluation: " << topMoves[0].first <<  endl;

    bestMove = topMoves.empty() ? Move() : topMoves[0].second.move;

    deleteTree(root);

    return bestMove;
}

int main()
{
    Board board;
    Move bestMove;

    board.turn = 1;

    if(!isBoardValid(board))   // Is board in checkmate & are king positions correct
        return 0;

    // Play against human
    Move playerMove;
    vector<Move> legalMoves;
    int x0, y0, x, y;
    bool validMove;     

    sf::RenderWindow window(sf::VideoMode(BOARD_SIZE * TILE_SIZE, BOARD_SIZE * TILE_SIZE), "Chess GUI");

    font.loadFromFile("dejavu.ttf");

    bool selecting = false;

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();

            if (event.type == sf::Event::MouseButtonPressed) {                
                int x = event.mouseButton.x / TILE_SIZE;
                int y = event.mouseButton.y / TILE_SIZE;

                // Take back move
                if (event.mouseButton.button == sf::Mouse::Right) {
                    cout << "Take back move" << endl;
                    board.moveBack();
                    board.moveBack();
                    drawBoard(window, board);
                    continue;
                }

                if (!selecting) {
                    x0 = x;
                    y0 = y;
                    selecting = true;
                } else {
                    int x1 = x;
                    int y1 = y;
                    Move playerMove = {board.turn, static_cast<uint8_t>(x0), static_cast<uint8_t>(y0), static_cast<uint8_t>(x1), static_cast<uint8_t>(y1), 0};

                    bool validMove = false;
                    std::vector<Move> legalMoves = findPlayerMoves(board);
                    for (const Move &m : legalMoves) {
                        if (m.x0 == playerMove.x0 && m.y0 == playerMove.y0 && m.x == playerMove.x && m.y == playerMove.y) {
                            validMove = true;
                            break;
                        }
                    }

                    if (validMove) {
                        board.move(playerMove);
                        
                        cout << "Player Move" << endl;
                        cout << moveToStr(playerMove) << endl;
                        board.printBoard(playerMove.x, playerMove.y);
                        
                        drawBoard(window, board);

                        // Handle end of game states
                        int state = boardState(board);
                        if (state == 3) {
                            std::cout << "Checkmate!\n";
                            drawBoard(window, board);
                            // Wait until window is closed
                            while (window.isOpen()) {
                                sf::Event event;
                                while (window.pollEvent(event)) {
                                    if (event.type == sf::Event::Closed)
                                        window.close();
                                }
                            }
                            return 0;
                        }
                        else if (state == 2) {
                            std::cout << "Stalemate!\n";
                            drawBoard(window, board);
                            // Wait until window is closed
                            while (window.isOpen()) {
                                sf::Event event;
                                while (window.pollEvent(event)) {
                                    if (event.type == sf::Event::Closed)
                                        window.close();
                                }
                            }
                            return 0;
                        }

                        cout << "Thinking..." << endl;
                        Move bestMove = getBestMove(board, maxDepth, searchLimit);
                        board.move(bestMove);
                        
                        cout << "Computer Move" << endl;
                        cout << moveToStr(bestMove) << endl;
                        board.printBoard(bestMove.x, bestMove.y);

                        // Handle end of game states
                        state = boardState(board);
                        if (state == 3) {
                            std::cout << "Checkmate!\n";
                            drawBoard(window, board);
                            // Wait until window is closed
                            while (window.isOpen()) {
                                sf::Event event;
                                while (window.pollEvent(event)) {
                                    if (event.type == sf::Event::Closed)
                                        window.close();
                                }
                            }
                            return 0;
                        }
                        else if (state == 2) {
                            std::cout << "Stalemate!\n";
                            drawBoard(window, board);
                            // Wait until window is closed
                            while (window.isOpen()) {
                                sf::Event event;
                                while (window.pollEvent(event)) {
                                    if (event.type == sf::Event::Closed)
                                        window.close();
                                }
                            }
                            return 0;
                        }
                    } else {
                        std::cout << "Invalid move!\n";
                    }
                    selecting = false;
                }
            }
        }
        drawBoard(window, board);
    }

    return 0;
}


// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
