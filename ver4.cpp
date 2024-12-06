#include <curses.h> 
#include <stdlib.h>
#include <iostream>
#include <time.h>
#include <cstring>
#include <stdio.h>

#define MAX_NUM 70
#define DELAY_CHANGE_T 4000     //a car changes its delay after 4-8 seconds (picked randomly)
#define CAR_HEIGHT 2
#define CAR_WIDTH 4
#define MAX_LINE_LENGTH 200
#define PROXIMITY 2    //if the frog is this distance from a car, provided the car is neutral, it will stop
#define INVINCIBILITY_TIME 500 //frog is immortal after getting out of a car
#define LEADERBOARD_FILE "leaderboard.txt"

typedef struct {
    char file_name[30];
    int car_number;
    int road_lanes;
    int min_car_delay, max_car_delay;
    int width;
    int height;
    char board[MAX_NUM][MAX_NUM];
    int f_car_chance;
    int n_car_chance;
} GameConfig;

typedef struct {
    int x, y;
    char direction;
    int delay;
    int last_move_time;
    bool hidden;
    char car_type;
    //'h' - hostile car, 'n' - neutral car, 'f' - friendly car
    //neutral cars stop the movement when the frog is close to them
    bool carrying_frog;
    clock_t hidden_until;
    clock_t until_delay_change;
} Car;

typedef struct {
    int x, y;
    char direction;
    int moves;
    int last_jump_time;
    int jump_delay;
    bool is_carried;
    bool is_invincible; //up to 0.5 seconds after getting out of a car the frog is "immortal" and can't die (so it can move away from the road)
    clock_t invincibility_start;
    int score;
    Car *frogs_car;
} Frog;

typedef struct {
    int x, y;
    int dir_x, dir_y;
    int delay;
    clock_t last_move_time;
    bool alive;
} Stork;


void delay(int mseconds) {
    clock_t start_time = clock();
    while (clock() < start_time + mseconds * CLOCKS_PER_SEC / 1000);
}

//FILE RELATED SECTION:
        //GETTING PARAMETERS FROM THE CONFIG FILE, PREPARING THE GAME
FILE* open_config(GameConfig *game_config){
    FILE *file = fopen(game_config->file_name, "r");
    if(!file){
        std::cerr << "There is no file with given name.\n";
    }
    return file;
}

bool parse_basic_data(char buffer[], GameConfig *game_config, Frog *frog, Stork *stork){
    int temp;
    if (sscanf(buffer, "jump_delay=%d", &frog->jump_delay) == 1) {
        return true;
    }
    if (sscanf(buffer, "road_lanes=%d", &game_config->road_lanes) == 1){ 
        return true;
    }
    if (sscanf(buffer, "n_car_chance=%d", &game_config->f_car_chance) == 1){
        return true;
    }
    if (sscanf(buffer, "f_car_chance=%d", &game_config->n_car_chance) == 1){
        return true;
    }
    if (sscanf(buffer, "car_number=%d", &game_config->car_number) == 1){
        return true;
    }
    if (sscanf(buffer, "width=%d", &game_config->width) == 1){ 
        return true;
    }
    if (sscanf(buffer, "height=%d", &game_config->height) == 1){ 
        return true;
    }
    if (sscanf(buffer, "stork_alive=%d", &temp) == 1){ 
        if(temp == 1){
            stork->alive = true;
        }
        else{
            stork->alive = false;
        }
        return true;
    }
    if (sscanf(buffer, "min_car_delay=%d", &game_config->min_car_delay) == 1) {
        return true;
    }
    if (sscanf(buffer, "max_car_delay=%d", &game_config->max_car_delay) == 1) {
        return true;
    }

    //if nothing from above has been found 
    return false;
}

bool parse_seed(FILE *file, char buffer[], GameConfig *game_config){
    for (int i = 0; i < game_config->height; i++) {
        if (!fgets(buffer, MAX_LINE_LENGTH, file)) {
            std::cerr << "Seed is too small, change height and width in the config file\n";
            return false;
        }
        for (int j = 0; j < game_config->width; j++) {
            game_config->board[i][j] = buffer[j];
        }
    }
    return true;
}

bool get_data(GameConfig *game_config, Frog *frog, FILE* file, Stork *stork){
    char buffer[MAX_LINE_LENGTH];
    bool is_seed_found = false;

    while(fgets(buffer, MAX_LINE_LENGTH, file)){
        if(buffer[0] == '\0' || buffer[0] == '\n'){
            continue;
        }

        if (parse_basic_data(buffer, game_config, frog, stork)) continue;

        if (strncmp(buffer, "seed=", 5) == 0) {
            is_seed_found = true;
            if (parse_seed(file, buffer, game_config) == false){
                 return false;
            }
        }
    }

    if (is_seed_found == false) {
        std::cerr << "No seed has been found in the given file.\n";
        return false;
    }
    return true;
}
bool read_config(GameConfig *game_config, Frog *frog, Stork *stork){
    FILE *file = open_config(game_config);

    if(get_data(game_config, frog, file, stork) == false){
        fclose(file);
        return false;
    }

    fclose(file);
    return true;
}

        //LEADERBOARD RELATED FUNCTIONS
void calculate_score(int time_elapsed, Frog *frog) {
    int base_score = 1000;            
    int time_penalty = time_elapsed * 10; 
    int move_penalty = frog->moves * 5;       

    frog->score = base_score - time_penalty - move_penalty;
    if (frog->score < 0) frog->score = 0;
}

void save_score(const char* player_name, int score) {
    FILE *file = fopen(LEADERBOARD_FILE, "a");
    if (!file) {
        std::cerr << "Something's wrong with the leaderboard file.\n";
        return;
    }
    fprintf(file, "%s - %d points.\n", player_name, score);
    fclose(file);
}

void print_ranking(int length, int score[], char name[][MAX_NUM], int sorted_indexes[]) {
    int pos = 6;
    clear();
    mvprintw(5, 10, "___=== Game Ranking ===___");
    for(int i = 0; i < length; i++){
        mvprintw(pos++, 10, "%d.) %s - %d points.", i + 1, name[sorted_indexes[i]], score[sorted_indexes[i]]);
    }
    mvprintw(pos + 2, 10, "Press anything to get back to the main menu.");
    refresh();
}

void sort_ranking_data(int length, char name[][MAX_NUM], int score[], int sorted_indexes[]){
    for(int i = 0; i < length; i++){
        sorted_indexes[i] = i;
    }

    for(int i = 0; i < length; i++){
        for(int j = 1; j < length; j++){
            if(score[sorted_indexes[j]] > score[sorted_indexes[j - 1]]){
                int temp = sorted_indexes[j];
                sorted_indexes[j] = sorted_indexes[j - 1];
                sorted_indexes[j - 1] = temp;
            }
        }
    }
}

void show_ranking() {
    FILE *file = fopen(LEADERBOARD_FILE, "r");
    if (!file) {
        std::cerr << "Something's wrong with the leaderboard file.\n";
        return;
    }

    char name[MAX_NUM][MAX_NUM]; //creates arrays that are to be sorted based on points
    int score[MAX_NUM];
    int length = 0;

    //data from file will be temporarily saved here
    char temp_name[MAX_NUM];
    int temp_score;
    while ((fscanf(file, "%s - %d points.", temp_name, &temp_score) == 2) && length < MAX_NUM) { 
        strcpy(name[length], temp_name);
        score[length] = temp_score;
        length++;
    }

    int *sorted_indexes = new int[length];
    sort_ranking_data(length, name, score, sorted_indexes);
    print_ranking(length, score, name, sorted_indexes);
    delete[] sorted_indexes;
   
    fclose(file);
}

void get_player_name(Frog *frog, char name[]) {
    mvprintw(3, 3, "Congratulations! You got: %d points.", frog->score);
    mvprintw(4, 5, "Enter your name:");
    echo();
    getnstr(name, MAX_NUM - 1); 
    noecho();
}

//INITIALIZING THE STORK
void init_stork(GameConfig *game_config, Stork *stork, Frog *frog){
    if(stork->alive == true){
        stork->x = rand() % (game_config->width / 2) + 1;
        stork->y = rand() % (game_config->height / 2) + game_config->height / 2;
        stork->delay = frog->jump_delay * 2;
        stork->last_move_time = clock();
        stork->dir_x = 1; 
        stork->dir_y = 1;
    }
}
//dir_x = -1; storks x position is decreasing
//dir_x = 0; storks x position is not changing
//dir_x = 1; storks x position is increasing
//same with dir_y

void check_whether_in_board(GameConfig *game_config, Stork *stork);
void set_storks_direction(Stork *stork, Frog *frog);

void move_stork(GameConfig *game_config, Stork *stork, Frog *frog) {
    if(stork->alive == true){
        if ((clock() - stork->last_move_time) * 1000 / CLOCKS_PER_SEC < stork->delay) {
            return; 
        }
        if(frog->frogs_car == NULL){
            set_storks_direction(stork, frog);

            stork->x += stork->dir_x;
            stork->y += stork->dir_y;

            check_whether_in_board(game_config, stork);
            stork->last_move_time = clock();
        }
    }
}

void set_storks_direction(Stork *stork, Frog *frog){
    if (frog->x > stork->x) {
        stork->dir_x = 1;
    } else if (frog->x < stork->x) {
        stork->dir_x = -1;
    } else {
        stork->dir_x = 0;
    }

    if (frog->y > stork->y) {
        stork->dir_y = 1;
    } else if (frog->y < stork->y) {
        stork->dir_y = -1;
    } else {
        stork->dir_y = 0;
    }
}

bool check_stork_collision(Frog *frog, Stork *stork) {
    if((frog->x == stork->x || frog->x + 1 == stork->x )&& frog->y == stork->y){
        return true;
    }
    else{
        return false;
    }
}

void check_whether_in_board(GameConfig *game_config, Stork *stork){
    if (stork->x <= 1) stork->x = 1;
    if (stork->x >= game_config->width) stork->x = game_config->width;
    if (stork->y <= 1) stork->y = 1;
    if (stork->y >= game_config->height) stork->y = game_config->height;
}


// INITIALIZING THE FROG
void init_frog(GameConfig* game_config, Frog* frog) {
    frog->x = game_config->width / 2 + 1;
    frog->y = game_config->height;
    frog->direction = 'U';                  // Initial frog's direction (upwards)
    frog->moves = 0;
    frog->last_jump_time = clock();
    frog->is_carried = false;
    frog->score = 0;
    frog->frogs_car = NULL;
}

// INITIALIZING AND RANDOMIZING CARS

void change_car_position(Car* car, GameConfig* game_config, int roads_pos[], int cars_on_lane[], int* free_lanes, int lane_direction[]) {
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
    } 
    else if(cars_on_lane[lane] == 0) {
        (*free_lanes)--; 
    }

    car->y = roads_pos[lane] + 1;
    car->direction = lane_direction[lane];
    cars_on_lane[lane]++;
}

void set_cars_type(Car *car, GameConfig *game_config){
    int temp = ((rand() % 100) * 7937) % 100;
        if(temp < game_config->f_car_chance){
            car->car_type = 'f';
        }
        else if(temp < (game_config->f_car_chance + game_config->n_car_chance)){
            car->car_type = 'n';
        }
        else{
            car->car_type = 'h';
        }
}
void init_cars(Car *cars, GameConfig *game_config, int roads_pos[], int cars_on_lane[], int *free_lanes, int lane_directions[]){
    for(int i = 0; i < game_config->car_number; i++){
        cars[i].x = (rand() % (game_config->width - 2)) + 2;
        cars[i].delay = (rand() % (game_config->max_car_delay - game_config->min_car_delay)) + game_config->min_car_delay;
        cars[i].last_move_time = clock();
        cars[i].hidden = false;
        cars[i].hidden_until = 0;
        cars[i].until_delay_change = ((rand() % DELAY_CHANGE_T) + DELAY_CHANGE_T) * CLOCKS_PER_SEC / 1000;
        set_cars_type(&cars[i], game_config);
        cars[i].carrying_frog = false;

        change_car_position(&cars[i], game_config, roads_pos, cars_on_lane, free_lanes, lane_directions);
    }
}

//INITIALIZING THE GAME

bool start_game() {
    initscr();             
    cbreak();              
    noecho();               
    keypad(stdscr, TRUE);   
    curs_set(0);            
    nodelay(stdscr, TRUE);
    srand(time(NULL));

    start_color();
    init_pair(1, 23, 23);           //colour for grass
    init_pair(2, 8, 8);             //colour for roads
    init_pair(3, COLOR_YELLOW, 10); //the frog
    init_pair(4, COLOR_WHITE, COLOR_BLACK);     //status bar
    init_pair(5, COLOR_YELLOW, 12);             //colour for hostile cars 
    init_pair(6, 10, 21);                       //obstacles
    init_pair(7, COLOR_BLACK, 14);              //neutral cars
    init_pair(8, COLOR_YELLOW, 11);             //friendly cars
    init_pair(9, COLOR_BLACK, COLOR_WHITE);       //stork

    return true;
}

// DRAWING SECTION - STATUS, BOARD, FROG, CARS, STORK

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
            else if (game_config->board[i - 1][j - 1] == 'O') {
                wattron(game_window, COLOR_PAIR(6));
                mvwprintw(game_window, i, j, "-");
                wattroff(game_window, COLOR_PAIR(6));
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

            //DRAWING DIFFERENT TYPES OF CARS
void draw_hostile_car(WINDOW *game_window, Car *car, GameConfig *game_config){
    wattron(game_window, COLOR_PAIR(5));
    if(car->direction == 1){
        mvwprintw(game_window, car->y, car->x, "' '*");
        mvwprintw(game_window, car->y + 1, car->x, ". .*");
    }
    else{
        mvwprintw(game_window, car->y, car->x, "*' '");
        mvwprintw(game_window, car->y + 1, car->x, "*. .");
    }
    wattroff(game_window, COLOR_PAIR(5));
}
void draw_neutral_car(WINDOW *game_window, Car *car, GameConfig *game_config){
    wattron(game_window, COLOR_PAIR(7));
    if(car->direction == 1){
        mvwprintw(game_window, car->y, car->x, "' '*");
        mvwprintw(game_window, car->y + 1, car->x, ". .*");
    }
    else{
        mvwprintw(game_window, car->y, car->x, "*' '");
        mvwprintw(game_window, car->y + 1, car->x, "*. .");
    }
    wattroff(game_window, COLOR_PAIR(7));
}
void draw_friendly_car(WINDOW *game_window, Car *car, GameConfig *game_config){
    wattron(game_window, COLOR_PAIR(8));
    if(car->direction == 1){
        mvwprintw(game_window, car->y, car->x, "' '*");
        mvwprintw(game_window, car->y + 1, car->x, ". .*");
    }
    else{
        mvwprintw(game_window, car->y, car->x, "*' '");
        mvwprintw(game_window, car->y + 1, car->x, "*. .");
    }
    wattroff(game_window, COLOR_PAIR(8));
}

void draw_carrying_car(WINDOW *game_window, Car *car, GameConfig *game_config){
    wattron(game_window, COLOR_PAIR(3));
    if(car->direction == 1){
        mvwprintw(game_window, car->y, car->x, "' '*");
        mvwprintw(game_window, car->y + 1, car->x, ". .*");
    }
    else{
        mvwprintw(game_window, car->y, car->x, "*' '");
        mvwprintw(game_window, car->y + 1, car->x, "*. .");
    }
    wattroff(game_window, COLOR_PAIR(3));
}

void draw_cars(WINDOW *game_window, Car *cars, GameConfig *game_config){
    for(int i = 0; i < game_config->car_number; i++){
        if(cars[i].hidden == true){
            continue;
        }

        if(cars[i].car_type == 'f' && cars[i].carrying_frog == true){
            draw_carrying_car(game_window, &cars[i], game_config);
        }
        else if(cars[i].car_type == 'h'){
            draw_hostile_car(game_window, &cars[i], game_config);
        }
        else if(cars[i].car_type == 'n'){
            draw_neutral_car(game_window, &cars[i], game_config);
        }
        else if(cars[i].car_type == 'f'){
            draw_friendly_car(game_window, &cars[i], game_config);
        }
    }
}

        //STORK DRAWING SECTION
void draw_stork(WINDOW *game_window, Stork *stork){
    if(stork->alive == true){
        wattron(game_window, COLOR_PAIR(9));
        mvwprintw(game_window, stork->y, stork->x, "V");
        mvwprintw(game_window, stork->y - 1, stork->x - 1, "\\ /"); 
        wattroff(game_window, COLOR_PAIR(9));
    }
}


// MOVEMENT SECTION OF FROG AND CARS

bool is_frog_near(Frog *frog, Car *car){
    int distance_x;
    if(car->direction == 1){
        distance_x = frog->x - (CAR_WIDTH - 2 + car->x);
    }
    else if(car->direction == -1){
        distance_x = frog->x - car->x ;
    }

    if(distance_x < 0){
        distance_x *= -1;
    }

    int distance_y = frog->y - car->y;
    if(frog->y < car->y){
        distance_y--;
    }

    if(distance_y < 0){
        distance_y *= -1;
    }

    if(distance_x <= PROXIMITY && distance_y <= PROXIMITY){
        return true;
    }
    else{
        return false;
    }
}

bool can_frog_jump(GameConfig *game_config, Frog *frog){
    if ((clock() - frog->last_jump_time) * 1000 / CLOCKS_PER_SEC >= frog->jump_delay) {      //Checks whether enough time has passed for frog to have another jump
        return true;                                                                         //If frog is not allowed to jump then terminate the function
    }
    else {
        return false;
    }
}
            //FROGS MOVES UP, DOWN, LEFT, RIGHT
void frog_move_up(GameConfig* game_config, Frog* frog){
    if (frog->y > 1) {
            if(game_config->board[frog->y - 1 - 1][frog->x] != 'O' && game_config->board[frog->y - 1 - 1][frog->x - 1] != 'O'){     //checking if frog doesn't want to jump onto an obstacle
                frog->y--;
                frog->moves++;
            }
        }
        frog->direction = 'U';
        frog->last_jump_time = clock();
}

void frog_move_down(GameConfig* game_config, Frog *frog){
    if (frog->y < game_config->height) {
            if(game_config->board[frog->y - 1 + 1][frog->x] != 'O' && game_config->board[frog->y - 1 + 1][frog->x - 1] != 'O'){      //checking if frog doesnt want to jump onto an obstacle
                frog->y++;
                frog->moves++;
            }
        }
        frog->direction = 'D';
        frog->last_jump_time = clock();
}

void frog_move_right(GameConfig *game_config, Frog* frog){
    if (frog->x < game_config->width - 2) {
            if(game_config->board[frog->y - 1][frog->x + 2] != 'O' && game_config->board[frog->y - 1][frog->x + 1] != 'O'){         //checking if frog doesnt want to jump onto an obstacle
                frog->x += 2;
                frog->moves++;
            } 
            else if(game_config->board[frog->y - 1][frog->x + 1] != 'O'){                           //if obstacle is half a normal movement in x-axis away then take a smaller jump
                frog->x += 1;
                frog->moves++;
            }
        }
        else if(frog->x < game_config->width - 1){                                                  //if game border is half a normal movement in x-axis away then do a smaller jump
            if(game_config->board[frog->y - 1][frog->x + 1] != 'O'){
                frog->x += 1;
                frog->moves++;
            }
        }
        frog->direction = 'R';
        frog->last_jump_time = clock();
}

void frog_move_left(GameConfig* game_config, Frog* frog){
    if (frog->x > 2) {
            if(game_config->board[frog->y - 1][frog->x - 3] != 'O' && game_config->board[frog->y - 1][frog->x - 2] != 'O'){         //checking if frog doesnt want to jump onto an obstacle
                frog->x -= 2;
                frog->moves++;
            } 
            else if(game_config->board[frog->y - 1][frog->x - 2] != 'O'){                            //if obstacle is half a normal movement in x-axis away then take a smaller jump
                frog->x -= 1;
                frog->moves++;
            }
        }
        else if(frog->x > 1){                                                                        //if game border is half a normal movement in x-axis away then do a smaller jump
            if(game_config->board[frog->y - 1][frog->x - 1] != 'O'){
                frog->x -= 1;
                frog->moves++;
            }
        }
        frog->direction = 'L';
        frog->last_jump_time = clock();
}

void frogs_move(GameConfig* game_config, Frog* frog, int movement) {
    if(can_frog_jump(game_config, frog) == false && movement != 'e' && movement != 't'){
        return;
    }
    else if(frog->is_carried == true){ //frog can't move when its being carried by a car
        return;
    }
    
    //If frog is allowed to jump, then update its position and last_jump_time for the present time (provided that it is not jumping onto an obstacle)
    //Frog is 2x wide, that's why there are two conditions for its x
    //We have to substract 1 from frog->y due to the board shift
    switch (movement) {
    case KEY_UP:
        frog_move_up(game_config, frog);
        break;
    case KEY_DOWN:
        frog_move_down(game_config, frog);
        break;
    case KEY_RIGHT:
        frog_move_right(game_config, frog);
        break;
    case KEY_LEFT:
        frog_move_left(game_config, frog);
        break;
    }

}


            //SECTION OF CARS MOVEMENT

bool car_visibility_check(GameConfig *game_config, Car *car, int roads_pos[], int cars_on_lane[], int *free_lanes, int lane_directions[]){
    if(car->hidden == true){
        if(clock() >= car->hidden_until){
            car->hidden = false;
            change_car_position(car, game_config, roads_pos, cars_on_lane, free_lanes, lane_directions);
        }
        else{
            return false;
        }
    }
    return true;
}

void manage_lanes(GameConfig *game_config, Car *car, int roads_pos[], int cars_on_lane[], int *free_lanes, int lane_directions[]){
    int current_lane = -1;
    for(int i = 0; i < game_config->road_lanes; i++){
        if(car->y == (roads_pos[i] + 1)){ 
            current_lane = i;
            break;
        }
    }

    //counting how many cars are on the lanes and how many lanes without cars there are
    cars_on_lane[current_lane]--;
            if(cars_on_lane[current_lane] == 0){
                (*free_lanes)++;
            }
            car->hidden = true;
            car->hidden_until = clock() + (rand() % 1000 + 500) * CLOCKS_PER_SEC / 1000;                   //random delay between 0.5 and 1.5 seconds
            car->x = game_config->width + 5;                                                               //placing car outside of the board so the frog won't step into it by an accident
            car->y = game_config->height + 5;
}

                //WHAT HAPPENS TO A CAR WHEN IT HITS THE BORDER
bool hits_the_border(GameConfig *game_config, Car *car){
    if(car->x <= 1 || (car->x + CAR_WIDTH - 2) >= game_config->width){
        return true;
    }
    else{
        return false;
    }
}

void cars_destiny(GameConfig *game_config, Car* car, int roads_pos[], int cars_on_lane[], int *free_lanes, int lane_directions[]){
    if(rand() % 3 == 0){
                if(car->direction == 1){
                    car->x = 1;
                }
                else{
                    car->x = game_config->width - CAR_WIDTH;
                }                                                                                               //the car wrapps (33% chance)
            }
            else{                                                                                               //the car changes lane (66% chance)
                manage_lanes(game_config, car, roads_pos, cars_on_lane, free_lanes, lane_directions);
            }
}

//MAIN FUNCTION OF CARS POSITION

bool cars_friendly_and_neutral_move(GameConfig *game_config, Frog *frog, Car *car){
    //friendly and neutral cars dont move when the frog is close and directed to them
    if (frog->direction == 'U' && (frog->y >= car->y)) {
        return false;
    }
    else if (frog->direction == 'D' && frog->y <= car->y + 1) {
        return false;
    }
    if (frog->direction == 'L' && (frog->x + 1 < car->x && car->direction == -1) || (frog->x > car->x + CAR_WIDTH - 1 && car->direction == 1)) {
        if(frog->y == car->y || frog->y == car->y + 1){
            return false;
        }
    }
    if (frog->direction == 'R' && (frog->x + 2 >= car->x && car->direction == -1) || (frog->x >= car->x + CAR_WIDTH - 1 && car->direction == 1)) {
        if(frog->y == car->y || frog->y == car->y + 1){
            return false;
        }
    }
    return true;
}

bool is_shant(GameConfig *game_config, Car *car, Car *cars){
    int car_i_pos = 0;
    for(int i = 0; i < game_config->car_number; i++){
        if(&cars[i] == car){
            car_i_pos = i;
        }
    }

    for(int i = 0; i < game_config->car_number; i++){
        if(i != car_i_pos){
            if (cars[i].y == car->y) { 
                if (car->direction == 1 && cars[i].x > car->x && cars[i].x - car->x <= CAR_WIDTH) {
                    return true;
                } 
                else if (car->direction == -1 && cars[i].x < car->x && car->x - cars[i].x <= CAR_WIDTH) {
                    return true;
                }
            }
        }
    }
    return false;
}

void update_car_pos(GameConfig *game_config, Car *car, Car *cars, Frog *frog, int roads_pos[], int cars_on_lane[], int *free_lanes, int lane_directions[]){
                                        //checks whether car should be shown
    if(car_visibility_check(game_config, car, roads_pos, cars_on_lane, free_lanes, lane_directions) == false){
        return;
    }

    if(car->car_type == 'n' && is_frog_near(frog, car) || (car->car_type == 'f' && is_frog_near(frog, car) && car->carrying_frog == false)){    
        //if(cars_friendly_and_neutral_move(game_config, frog, car) == false){
        return;
        //}
    }

    if(is_shant(game_config, car, cars) == true){ //if a car would ride "into" a car that is ahead of it then stop its movement
        return;
    }

    if(car->carrying_frog == false){
        car->x += car->direction;

        if(hits_the_border(game_config, car) == true){
            cars_destiny(game_config, car, roads_pos, cars_on_lane, free_lanes, lane_directions);
        }
    }
    else{                                  //when a car carries the frog then it can't dissapear when close to the border, it has to wait for frog to get out of the car
         if(car->x + 1 > 1 && (car->x + CAR_WIDTH - 1) < game_config->width){
            car->x += car->direction;
        }
        else{
            return;
        }
    }
}

void change_car_delay(GameConfig *game_config, Car *car){
    if(clock() >= car->until_delay_change){
        car->delay = (rand() % (game_config->max_car_delay - game_config->min_car_delay)) + game_config->min_car_delay;
        
        car->until_delay_change = clock() + ((rand() % DELAY_CHANGE_T) + DELAY_CHANGE_T) * CLOCKS_PER_SEC / 1000;
    }
}

void cars_move(GameConfig *game_config, Car* cars, Frog* frog, int roads_pos[], int cars_on_lane[], int *free_lanes, int lane_directions[]){
    for(int i = 0; i < game_config->car_number; i++){
        if((clock() - cars[i].last_move_time) * 1000 / CLOCKS_PER_SEC >= cars[i].delay){
            //updates cars position on the board
            update_car_pos(game_config, &cars[i], cars, frog, roads_pos, cars_on_lane, free_lanes, lane_directions);
            cars[i].last_move_time = clock();

            //checks whether enought time has passed for car to have another delay (meaning another speed)
            change_car_delay(game_config, &cars[i]);
        }
    }
}


// MAIN GAME CONDITIONS - WHETHER FROG IS STILL ALIVE OR NOT

void update_invincibility(Frog *frog) {
    if (frog->is_invincible) {
        if ((clock() - frog->invincibility_start) * 1000 / CLOCKS_PER_SEC >= INVINCIBILITY_TIME) { 
            frog->is_invincible = false;
        }
    }
}

bool check_collision(Frog *frog, Car *cars, GameConfig *game_config){
    if(frog->is_carried == true){
        return false;
    }
    if(frog->is_invincible == true){
        return false;
    }

    for(int i = 0; i < game_config->car_number; i++){
        int frog_left = frog->x;
        int frog_right = frog->x + 1;
        int frog_y_axis = frog->y;

        int car_left = cars[i].x;
        int car_right = cars[i].x + CAR_WIDTH - 1;
        int car_top = cars[i].y;
        int car_bottom = cars[i].y + CAR_HEIGHT - 1;

        if (frog_right >= car_left && frog_left <= car_right &&
            frog_y_axis >= car_top && frog_y_axis <= car_bottom) {
            return true;
        }
    }
    return false;
}

char check_game_status(WINDOW* game_window, GameConfig* game_config, Frog* frog, Car* cars, Stork *stork, int time_elapsed) {           //n - nothing's changed; l - game lost; w - game won
    if (frog->y == 1) {
        mvwprintw(game_window, game_config->height / 2, game_config->width / 2 - 5, "YOU WON!");
        wrefresh(game_window);
        calculate_score(time_elapsed, frog);

        char name[MAX_NUM];
        get_player_name(frog, name); 
        save_score(name, frog->score);   
        delay(1000);
        return 'w';
    }
    if(check_collision(frog, cars, game_config) == true){
        mvwprintw(game_window, game_config->height / 2, game_config-> width /2 - 11, "GAME OVER!\tYOU LOST!");
        wrefresh(game_window);
        delay(2000);
        return 'c';
    }
    if (check_stork_collision(frog, stork)) {
        mvwprintw(game_window, game_config->height / 2, game_config->width / 2 - 11, "GAME OVER!\tSTORK GOT YOU!");
        wrefresh(game_window);
        delay(2000);
    return false; // Gra kończy się
}
    return 'n';
}

//MAIN GAME LOOP

            //FROG AND FRIENDLY CARS
Car *find_near_friendly_car(GameConfig *game_config, Frog* frog, Car *cars){
    Car *car = NULL;
    for(int i = 0; i < game_config->car_number; i++){
        if(cars[i].car_type == 'f'){
            if(is_frog_near(frog, &cars[i]) == true){
                car = &cars[i];
            }
        }
    }

    return car;
}

void frog_gets_in_the_car(GameConfig *game_config, Frog *frog, Car *friendly_car){
    if(friendly_car != NULL){
        friendly_car->carrying_frog = true;
        frog->is_carried = true;
        frog->x = game_config->width / 2;
        frog->y = game_config->height + 1;
        frog->frogs_car = friendly_car;
    }
    else{
        return;
    }
}

void frog_gets_out_of_the_car(GameConfig *game_config, Frog *frog){
    if(frog->frogs_car != NULL){
        frog->frogs_car->carrying_frog = false;
        frog->is_carried = false;
        if(frog->frogs_car->direction == 1){
            frog->x = frog->frogs_car->x - 1;
        }
        else{
            frog->x = frog->frogs_car->x + CAR_WIDTH + 1;
        }
        frog->y = frog->frogs_car->y;
        frog->is_invincible = true;
        frog->invincibility_start = clock();

        
        frog->frogs_car = NULL;
        return;
    }
    else{
        return;
    }
}

bool game_update(WINDOW* game_window, GameConfig* game_config, Frog* frog, Car *cars, Stork* stork, clock_t start_time, int time_elapsed, int roads_pos[], int cars_on_lane[], int *free_lanes, int lane_directions[]){
    Car *friendly_car = find_near_friendly_car(game_config, frog, cars);        //if there is a friendly car in proximity of the frog then it is being saved into this variable

    int movement = getch();
    if (movement == 'q') {
        return false;
    }
    else if (movement == 'i' && frog->is_carried == false){
        frog_gets_in_the_car(game_config, frog, friendly_car);
        frog->frogs_car = friendly_car;
    }
    else if(movement == 'o'){
        frog_gets_out_of_the_car(game_config, frog);
    }
    else{
        frogs_move(game_config, frog, movement);
    }
    
    cars_move(game_config, cars, frog, roads_pos, cars_on_lane, free_lanes, lane_directions);
    move_stork(game_config, stork, frog);
    update_invincibility(frog);

                        //drawing
    //clear();
    draw_board(game_window, game_config);
    if(frog->is_carried == false){
        draw_frog(game_window, frog);
    }
    draw_cars(game_window, cars, game_config);
    draw_status(game_config, frog, time_elapsed);
    draw_stork(game_window, stork);
    return true;
}

char game_play(WINDOW* game_window, GameConfig* game_config, Frog* frog, Car *cars, Car** frogs_car, Stork *stork, clock_t start_time, int roads_pos[], int cars_on_lane[], int *free_lanes, int lane_directions[]) {
    for (;;) {
        int time_elapsed = (clock() - start_time) / CLOCKS_PER_SEC;             //counting past time
        if(game_update(game_window, game_config, frog, cars, stork, start_time, time_elapsed, roads_pos, cars_on_lane, free_lanes, lane_directions) == false){
            return false;
        }

        if (check_game_status(game_window, game_config, frog, cars, stork, time_elapsed) != 'n') { //if game is won or lost, the function has to be finished executing
            return false;
        }

        wrefresh(game_window);
        delay(40); // Małe opóźnienie pętli
    }
}

//PREPARING TO START THE GAME

bool initialize_game(GameConfig* game_config, Frog* frog) {
    game_config->car_number = 1;
    game_config->f_car_chance = 0;
    game_config->n_car_chance = 0;
    if (!start_game()) {
        return false;
    }
    return true;
}

void setup_roads(GameConfig* game_config, int roads_pos[]) {
    int temp = 0;
    for (int i = 0; i < game_config->height; i++) {
        if (game_config->board[i][0] == 'R') {
            roads_pos[temp] = i;
            temp++;
            i++;
        }
    }
}

int* setup_cars_on_lane(GameConfig* game_config) {
    int* cars_on_lane = new int[game_config->road_lanes];
    for (int i = 0; i < game_config->road_lanes; i++) {
        cars_on_lane[i] = 0;
    }
    return cars_on_lane;
}

int* setup_lane_directions(GameConfig* game_config) {
    int* lane_directions = new int[game_config->road_lanes];
    for (int i = 0; i < game_config->road_lanes; i++) {
        if(rand() % 2 == 0){
            lane_directions[i] = 1;
        }
        else {
            lane_directions[i] = -1;
        }
    }
    return lane_directions;
}

void cleanup_game(Frog* frog, Car* cars, Stork *stork, int* cars_on_lane, int* lane_directions) {
    delete frog;
    delete[] cars;
    delete[] cars_on_lane;
    delete[] lane_directions;
    delete[] stork;
    endwin();
}

//CREATING THE MENU PAGE

void display_menu() {
    clear();
    mvprintw(5, 10, "___=== Game Menu ===___");
    mvprintw(6, 10, "1. Start Game");
    mvprintw(7, 10, "2. Show Leaderboard");
    mvprintw(8, 10, "3. Game Rules");
    mvprintw(9, 10, "4. Exit");
    mvprintw(11, 10, "Choose an option from 1 to 4:");
    refresh();
}

void show_levels() {
    clear();
    mvprintw(5, 10, "___=== Select Difficulty ===___");
    mvprintw(6, 10, "1. Easy level");
    mvprintw(7, 10, "2. Medium level");
    mvprintw(8, 10, "3. Difficult level");
    mvprintw(10, 10, "Choose an option from 1 to 3:");
    refresh();
}

bool handle_level_choice(int choice, char config_file_name[]) {
    switch (choice) {
        case 1: // Name of the easy level config file
            strcpy(config_file_name, "config_easy.txt");
            return true;
        case 2: // -=- medium
            strcpy(config_file_name, "config_medium.txt");
            return true;
        case 3: // -=- difficult
            strcpy(config_file_name, "config_difficult.txt");
            return true;
        default:
            return false;
            refresh();
            getch();
    }
    return false;
}

void print_game_rules(){
    clear();
    mvprintw(5, 10, "___=== Game Rules ===___");
    mvprintw(6, 10, "1) Move with arrows;");
    mvprintw(7, 10, "2) Red cars are hostile ones;");
    mvprintw(8, 10, "3) Yellow cars are neutral, they will wait for you to pass them by;");
    mvprintw(9, 10, "4) Same with blue cars, but they can also take you to another place");
    mvprintw(10, 10, "\t4.1) If you want to hop into a car you need to press 'i'");
    mvprintw(11, 10, "\t\ton your keyboard when you are close to them. Your car turns green.");
    mvprintw(12, 10, "\t4.2) In order to get out of the car you need to press 'o';");
    mvprintw(13, 10, "5) If you want to quit the game press 'q'.");
    mvprintw(15, 10, "Press anything to get back to the main menu.");
    refresh();
}
char handle_menu_choice(int choice, char config_file_name[]) { 
    switch (choice) {
        case 1: {
            int level_choice;
            show_levels();
            level_choice = getch() - '0';
            if (handle_level_choice(level_choice, config_file_name) == true) {
                return 's';
            }
            break;
        }
        case 2:
            clear();
            show_ranking();
            refresh();
            getch();
            break;
        case 3:
            print_game_rules();
            getch();
            break;
        case 4:
            return 'e';
        default:
            mvprintw(12, 10, "Invalid choice, pick a number from 1 to 4.");
            refresh();
            getch();
    }
    return 'n';
}

int play(GameConfig *game_config) {
    Frog* frog = new Frog;
    Stork *stork = new Stork;
    game_config->car_number = 1;
    stork->alive = false;

    if (read_config(game_config, frog, stork) == false) {
        return 0;
    }

    int roads_pos[MAX_NUM] = {0};
    setup_roads(game_config, roads_pos);

    int* cars_on_lane = setup_cars_on_lane(game_config);
    int free_lanes = game_config->road_lanes;
    int* lane_directions = setup_lane_directions(game_config);

    Car *cars = new Car[game_config->car_number];
    Car *frogs_car = NULL;
    init_frog(game_config, frog);
    init_cars(cars, game_config, roads_pos, cars_on_lane, &free_lanes, lane_directions);
    init_stork(game_config, stork, frog);

    clock_t start_time = clock(); //Time of the beginning of the game
    clock_t last_move_time = clock();
    WINDOW* game_window = newwin(game_config->height + 2, game_config->width + 2, 0, 0); 

    if (game_play(game_window, game_config, frog, cars, &frogs_car, stork, start_time, roads_pos, cars_on_lane, &free_lanes, lane_directions) == false) {
        cleanup_game(frog, cars, stork, cars_on_lane, lane_directions);    
    }
    return 1;
}

int main() {
    start_game(); //getting pdcurses to work

    GameConfig *game_config = new GameConfig;
    char config_file_name[MAX_NUM];

    nodelay(stdscr, FALSE); //getch waits for the users input
    while (true) {
        display_menu();
        int choice = getch() - '0';
        char action = handle_menu_choice(choice, config_file_name);
        if (action == 'e') {
            delete game_config;
            break;
        }
        else if(action == 's'){
            nodelay(stdscr, TRUE); //now the program works without the need of intervention from the player 
            strcpy(game_config->file_name, config_file_name);
            if(play(game_config) == 0){
                std::cerr << "Somethings wrong with the given data in the config file.";
            }
            nodelay(stdscr, FALSE); //again, now program waits for the users input
        }
    }

    endwin();
    return 0;
}