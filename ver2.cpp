#include <curses.h> 
#include <stdlib.h>
#include <iostream>
#include <time.h>
#include <cstring>
#include <windows.h>
#include <stdio.h>
//naprawienie losowania pozycji aut w porównaniu do pliku gra.cpp
#define MAX_NUM 70
#define CAR_HEIGHT 2
#define CAR_WIDTH 4

typedef struct {
    char file_name[30];
    int jump_delay;
    int car_number;
    int road_lanes;
    int min_car_delay, max_car_delay;

    int width;
    int height;
    char board[MAX_NUM][MAX_NUM];
} GameConfig;

typedef struct {
    int x, y;
    char direction;
    int moves;
    int last_jump_time;
    int jump_delay;
} Frog;

typedef struct {
    int x, y;
    char direction;
    int delay;
    int last_move_time;
} Car;


void delay(int mseconds) {
    clock_t start_time = clock();
    while (clock() < start_time + mseconds * CLOCKS_PER_SEC / 1000);
}



bool read_config(GameConfig* config, Frog* frog) {
    FILE* file = fopen(config->file_name, "r");
    if (!file) {
        std::cerr << "File " << config->file_name << " not found.\n";
        return false;
    }

    char buffer[100];
    bool flag_is_seed_found = false;

    while (fgets(buffer, sizeof(buffer), file)) {
        if (buffer[0] == '\n' || buffer[0] == '\0') continue;

        if (sscanf(buffer, "jump_delay=%d", &config->jump_delay) == 1) {
            frog->jump_delay = config->jump_delay;
        }
        else if (sscanf(buffer, "road_lanes=%d", &config->road_lanes) == 1) {
            // nothing's here
        }
        else if (sscanf(buffer, "car_number=%d", &config->car_number) == 1) {
            // nothing's here
        }
        else if (sscanf(buffer, "width=%d", &config->width) == 1) {
            // nothing's here
        }
        else if (sscanf(buffer, "height=%d", &config->height) == 1) {
            // nothing's here
        }
        else if (sscanf(buffer, "min_car_delay=%d", &config->min_car_delay) == 1) {
            // nothing's here
        }
        else if (sscanf(buffer, "max_car_delay=%d", &config->max_car_delay) == 1) {
            // nothing's here
        }
        else if (strncmp(buffer, "seed=", 5) == 0) {
            flag_is_seed_found = true;
            for (int i = 0; i < config->height; i++) {
                if (fgets(buffer, sizeof(buffer), file)) {
                    for (int j = 0; j < config->width; j++) {
                        if (j < (int)strlen(buffer)) {
                            config->board[i][j] = buffer[j];
                        }
                        else {
                            config->board[i][j] = 'G';                      //if nothing's written in the config.txt file in the checked place, 
                                                                            //replace empty character on the board with a grass field
                        }
                    }
                }
                else {
                    fclose(file);
                    std::cerr << "Seed is too small - adjust height and width in the config file\n";
                    return false;
                }
            }
        }
    }

    fclose(file);
    if (flag_is_seed_found == false) {
        std::cerr << "No seed has been found.\n";
        return false;
    }
    return true;
}

void init_frog(GameConfig* game_config, Frog* frog) {
    frog->x = game_config->width / 2 + 1;
    frog->y = game_config->height;
    frog->direction = 'U';                  // Initial frog's direction (upwards)
    frog->moves = 0;
    frog->last_jump_time = clock();
}

// INITIALIZING AND RANDOMIZING CARS

void change_car_position(Car* car, GameConfig* game_config, int roads_pos[], int cars_on_lane[], int* free_lanes) {
    int lane = -1;

    // Find an empty lane if exists
    for (int i = 0; i < game_config->road_lanes; i++) {
        if (cars_on_lane[i] == 0) {
            lane = i;
            break;
        }
    }

    // If there is none empty lanes pick random one
    if (lane == -1) {
        lane = rand() % game_config->road_lanes;
    } else if(cars_on_lane[lane] == 0) {
        (*free_lanes)--; 
    }

    car->y = roads_pos[lane] + 1;
    cars_on_lane[lane]++;
}

void init_cars(Car *cars, GameConfig *game_config, int roads_pos[], int cars_on_lane[], int *free_lanes){
    for(int i = 0; i < game_config->car_number; i++){
        cars[i].x = (rand() % (game_config->width - 2)) + 2;
        cars[i].delay = (rand() % (game_config->max_car_delay - game_config->min_car_delay)) + game_config->min_car_delay;
        cars[i].last_move_time = clock();

        if(rand() % 2 == 0){
            cars[i].direction = 1;
        }
        else{
            cars[i].direction = -1;
        }

        change_car_position(&cars[i], game_config, roads_pos, cars_on_lane, free_lanes);
    }
}

//INITIALIZING THE GAME

bool start_game(GameConfig* game_config, Frog* frog) {
    initscr();              // Inicjalizacja PDCurses
    cbreak();               // Tryb znak-po-znaku
    noecho();               // Wyłączenie echa wpisanych znaków
    keypad(stdscr, TRUE);   // Obsługa klawiatury (np. strzałek)
    curs_set(0);            // Ukrycie kursora
    nodelay(stdscr, TRUE);
    srand(time(NULL));

    start_color();
    init_pair(1, 23, 23);     // Grass
    init_pair(2, 8, 8);     // Road
    init_pair(3, COLOR_YELLOW, 10);    // Frog
    init_pair(4, COLOR_WHITE, COLOR_BLACK);     // Status
    init_pair(5, COLOR_YELLOW, COLOR_RED);

    strcpy(game_config->file_name,"config.txt");
    if (read_config(game_config, frog) == false) {
        return false;
    }

    return true;
}

// DRAWING SECTION - STATUS, BOARD, FROG, CARS

void draw_status(GameConfig* game_config, Frog* frog, int time_elapsed) {
    attron(COLOR_PAIR(4));
    mvprintw(game_config->height + 2, 0, "Jakub Sledzik | ID: 203221 | Ruchy: %d | Czas: %ds", frog->moves, time_elapsed);
    attroff(COLOR_PAIR(4));
}

void draw_board(WINDOW* game_window, GameConfig* game_config) {
    box(game_window, 0, 0);

    //Colouring grass and road fields on the board with matching color_pairs
    for (int i = 1; i <= game_config->height; i++) {
        for (int j = 1; j <= game_config->width; j++) {
            if (game_config->board[i - 1][j - 1] == 'R') {
                wattron(game_window, COLOR_PAIR(2));
                mvwprintw(game_window, i, j, " ");
                wattroff(game_window, COLOR_PAIR(2));
            }
            else if (game_config->board[i - 1][j - 1] == 'G') {
                wattron(game_window, COLOR_PAIR(1));
                mvwprintw(game_window, i, j, " ");
                wattroff(game_window, COLOR_PAIR(1));
            }
        }
    }

    wrefresh(game_window);
}

void draw_frog(WINDOW* game_window, Frog* frog) {
    wattron(game_window, COLOR_PAIR(3));
    if (frog->direction == 'U') {
        mvwprintw(game_window, frog->y, frog->x, "''");
    }
    else if (frog->direction == 'D') {
        mvwprintw(game_window, frog->y, frog->x, "..");
    }
    else if (frog->direction == 'R') {
        mvwprintw(game_window, frog->y, frog->x, " =");
    }
    else {
        mvwprintw(game_window, frog->y, frog->x, "= ");
    }
    wattroff(game_window, COLOR_PAIR(3));
}

void draw_cars(WINDOW *game_window, Car *cars, GameConfig *game_config){
    wattron(game_window, COLOR_PAIR(5));
    for(int i = 0; i < game_config->car_number; i++){
        if(cars[i].direction == 1){
            mvwprintw(game_window, cars[i].y, cars[i].x, "' '*");
            mvwprintw(game_window, cars[i].y + 1, cars[i].x, ". .*");
        }
        else{
            mvwprintw(game_window, cars[i].y, cars[i].x, "*' '");
            mvwprintw(game_window, cars[i].y + 1, cars[i].x, "*. .");
        }
    }
    wattroff(game_window, COLOR_PAIR(5));
}

// MOVEMENT SECTION OF FROG AND CARS

void frogs_move(GameConfig* game_config, Frog* frog, int movement) {
    if ((clock() - frog->last_jump_time) * 1000 / CLOCKS_PER_SEC < frog->jump_delay) {  //Checks whether enough time has passed for frog to have another jump
        return;                                                                         //If frog is not allowed to jump then terminate the function
    }
    //If frog is allowed to jump, then update its position and last_jump_time for the present time
    switch (movement) {
    case KEY_UP:
        if (frog->y > 1) {
            frog->y--;
            frog->moves++;
        }
        frog->direction = 'U';
        frog->last_jump_time = clock();
        break;
    case KEY_DOWN:
        if (frog->y < game_config->height) {
            frog->y++;
            frog->moves++;
        }
        frog->direction = 'D';
        frog->last_jump_time = clock();
        break;
    case KEY_RIGHT:
        if (frog->x < game_config->width - 2) {
            frog->x += 2;
            frog->moves++;
        }
        frog->direction = 'R';
        frog->last_jump_time = clock();
        break;
    case KEY_LEFT:
        if (frog->x > 2) {
            frog->x -= 2;
            frog->moves++;
        }
        frog->direction = 'L';
        frog->last_jump_time = clock();
        break;
    }

}

void update_car_pos(GameConfig *game_config, Car *car, int roads_pos[], int cars_on_lane[], int *free_lanes){ //there's 33% chance for car to turn around or to change lane
int current_lane = -1;
for(int i = 0; i < game_config->road_lanes; i++){
    if(car->y == (roads_pos[i] + 1)){ 
        current_lane = i;
        break;
    }
}
    car->x += car->direction;
    if(car->x <= 1 || (car->x + CAR_WIDTH - 2) >= game_config->width){
        if(rand() % 3 == 0){
            car->direction *= -1;                                                                           //the car turns around
        }
        else{                                                                                               //the car changes lane
            cars_on_lane[current_lane]--;
            if(cars_on_lane[current_lane] == 0){
                (*free_lanes)++;
            }
            change_car_position(car, game_config, roads_pos, cars_on_lane, free_lanes);
        }

        if(car->direction == 1){
            car->x = 1;
        }
        else{
            car->x = game_config->width - CAR_WIDTH;
        }
    }
}

void cars_move(GameConfig *game_config, Car* cars, int roads_pos[], int cars_on_lane[], int *free_lanes){
    for(int i = 0; i < game_config->car_number; i++){
        if((clock() - cars[i].last_move_time) * 1000 / CLOCKS_PER_SEC >= cars[i].delay){
            update_car_pos(game_config, &cars[i], roads_pos, cars_on_lane, free_lanes);
            cars[i].last_move_time = clock();
        }
    }
}

// MAIN GAME CONDITIONS - WHETHER FROG IS STILL ALIVE OR NOT

bool check_collision(Frog *frog, Car *cars, GameConfig *game_config){
    for(int i = 0; i < game_config->car_number; i++){
        if(frog->y >= cars[i].y && frog->y < cars[i].y + CAR_HEIGHT &&
            frog->x >= cars[i].x && frog->x < cars[i].x + CAR_WIDTH){
                return true;
            }
    }
    return false;
}

char check_game_status(WINDOW* game_window, GameConfig* game_config, Frog* frog, Car* cars) {           //n - nothing's changed; l - game lost; w - game won
    if (frog->y == 1) {
        mvwprintw(game_window, game_config->height / 2, game_config->width / 2 - 5, "YOU WON!");
        wrefresh(game_window);
        delay(2000);
        return 'w';
    }
    if(check_collision(frog, cars, game_config) == true){
        mvwprintw(game_window, game_config->height / 2, game_config-> width /2 - 11, "GAME OVER!\tYOU LOST!");
        wrefresh(game_window);
        delay(2000);
        return 'c';
    }
    return 'n';
}

//MAIN GAME LOOP

char game_play(WINDOW* game_window, GameConfig* game_config, Frog* frog, Car *cars, clock_t start_time, int roads_pos[], int cars_on_lane[], int *free_lanes) {
    for (;;) {
        int time_elapsed = (clock() - start_time) / CLOCKS_PER_SEC;             //counting past time

        int movement = getch();
        if (movement == 'q') {
            return '0';
        }

        frogs_move(game_config, frog, movement);
        cars_move(game_config, cars, roads_pos, cars_on_lane, free_lanes);

        // Odświeżanie ekranu
        // clear();
        draw_board(game_window, game_config);
        draw_frog(game_window, frog);
        draw_cars(game_window, cars, game_config);
        draw_status(game_config, frog, time_elapsed);


        if (check_game_status(game_window, game_config, frog, cars) != 'n') { //if game is won or lost, the function has to be finished executing
            return '0';
        }


        wrefresh(game_window);
        delay(40); // Małe opóźnienie pętli
    }
}

int main() {
    Frog* frog = new Frog;
    GameConfig* game_config = new GameConfig;
    game_config->car_number = 1;

    if (start_game(game_config, frog) == false) {
        return 0;
    }

    int roads_pos[MAX_NUM] = {0};
    int temp = 0;
    for (int i = 0; i < game_config->height; i++) {
        if (game_config->board[i][0] == 'R') {
            roads_pos[temp] = i;
            temp++;
            i++;
        }
    }

    int* cars_on_lane = new int[game_config->road_lanes];
    for (int i = 0; i < game_config->road_lanes; i++) {
        cars_on_lane[i] = 0;
    }
    int free_lanes = game_config->road_lanes;

    Car *cars = new Car[game_config->car_number];
    init_frog(game_config, frog);
    init_cars(cars, game_config, roads_pos, cars_on_lane, &free_lanes);

    clock_t start_time = clock(); //Time of the beginning of the game
    clock_t last_move_time = clock();
    WINDOW* game_window = newwin(game_config->height + 2, game_config->width + 2, 0, 0); 

    if (game_play(game_window, game_config, frog, cars, start_time, roads_pos, cars_on_lane, &free_lanes) == '0') {
        delete frog;
        delete game_config;
        delete[] cars;
        delete[] cars_on_lane;

        endwin();     
        return 0;
    }
   
}