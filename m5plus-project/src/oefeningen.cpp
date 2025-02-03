#include <M5StickCPlus.h>
#include <stdint.h>
#include "EEPROM.h"
#include <stdio.h>
#include <stdlib.h>

#define MEM_SIZE 512
uint8_t SCREEN_WIDTH = 8;
uint8_t SCREEN_HEIGHT = 16;
uint8_t SQUARE_WIDTH = M5.Lcd.width() / SCREEN_WIDTH;
uint8_t SQUARE_HEIGHT = M5.Lcd.height() / SCREEN_HEIGHT;

uint32_t L_RED = M5.Lcd.color565(255, 80, 80);
uint32_t L_BLUE = M5.Lcd.color565(0, 153, 204);
uint32_t L_YELLOW = M5.Lcd.color565(255, 255, 102);

uint8_t color_1;
uint8_t color_2;
uint8_t next_color_1 = rand() % 3;
uint8_t next_color_2 = rand() % 3;

////////////Which Color////////////////
uint32_t which_color(int color, int type)
{ 
  if (color == 0)
  {
    if (type == 6)
    return L_RED;
    else return RED;
  }
  else if (color == 1)
  {
    if (type == 6)
    return L_BLUE;
    return BLUE;
  }
  else if (color == 2)
  {
    if (type == 6)
    return L_YELLOW;
    return YELLOW;
  }
  else if (color == 3)
  {
    return BLACK;
  }
  else if (color == 4)
  {
    return GREEN;
  }
  else
    return BLACK;
}
//////Function declaration///////
int valid_move(char direction, int y, int x, int y2, int x2, int med_state, bool drop_med);
void rotate();
void drop_medecin();
void four_in_a_row();
void setup();
void clear_field();
void new_medecin();

struct Block
{
  uint8_t end_state; //1-4 
  uint8_t pair_x;
  uint8_t pair_y;
  uint8_t type; // 0 stand for main medecin that is still in pair, 1 stand for second medecin that is still in pair, 2 for virus, 3 for empty, 4 for a broken medecin that can fall, 5 for a bomb, 6 timed_virus
  uint8_t color; // 0 RED, 1 BLUE, 2 YELLOW, 3 BLACK, 4 GREEN
};

Block Empty_Block = {0, 0, 0, 3, 3};
Block Bomb = {0, 0, 0, 5, 4};
Block Timed_Virus = {0, 0, 0, 6, 3};

// Create matrix//
Block **field;

void create_matrix()
{
  field = new Block *[SCREEN_HEIGHT];
  for (int i = 0; i < SCREEN_HEIGHT; i++)
  {
    field[i] = new Block[SCREEN_WIDTH];
  }
}
/// Delete matrix //
void delete_matrix()
{
  for (int i = 0; i < SCREEN_HEIGHT; i++)
  {
    delete[] field[i];
  }
  delete[] field;
}

uint8_t x, y;
uint8_t x2, y2;
uint8_t score = 0;
uint8_t virus_count = 0;
uint8_t countdown;
uint8_t turn;
uint8_t gravity_time = 10;
bool game_over = false;
bool countdown_active = false;
uint8_t medecine_state = 1; // This means it's never been turned it will go to 2,3,4 for each possible turn
uint8_t virus_rarity = 50;

// MENU OPTIONS//
bool Bombs_enabled = false;
bool Timed_Virus_enabled = false;

////////////SAVE GAME///////////////
uint32_t encode_block(Block block)
{
  uint32_t shifted_end_state = block.end_state << 29;
  uint32_t shifted_pair_x = block.pair_x << 21;
  uint32_t shifted_pair_y = block.pair_y << 13;
  uint32_t shifted_type = block.type << 10;
  uint32_t shifted_color = block.color << 7;
  uint32_t encoded = (shifted_end_state | shifted_pair_x |
                     shifted_pair_y | shifted_type | shifted_color) << 0;
  return encoded;
}

Block decode_block(uint32_t encoded)
{
  Block block;
  block.end_state = (encoded >> 29) & 0x7;
  block.pair_x = (encoded >> 21) & 0xF;
  block.pair_y = (encoded >> 13) & 0xF;
  block.type = (encoded >> 10) & 0x7;
  block.color = (encoded >> 7) & 0x7;
  return block;
}

void saveGame()
{
  EEPROM.put(0, Bombs_enabled);
  EEPROM.put(1, Timed_Virus_enabled);
  EEPROM.put(2, score);
  EEPROM.put(3, turn);
  EEPROM.put(4, x);
  EEPROM.put(5, y);
  EEPROM.put(6, x2);
  EEPROM.put(7, y2);
  EEPROM.put(8, countdown_active);
  EEPROM.put(9, countdown);
  EEPROM.put(10, virus_count);
  EEPROM.put(11, medecine_state);
  EEPROM.put(12, SCREEN_HEIGHT);
  EEPROM.put(13, SCREEN_WIDTH);
  EEPROM.put(14, color_1);
  EEPROM.put(15, color_2);
  EEPROM.put(16, next_color_1);
  EEPROM.put(17, next_color_2);
  EEPROM.put(18, virus_rarity);
  int address = 19; // start storing matrix elements at address 19
  for (int i = 0; i < SCREEN_HEIGHT; i++) {
    for (int j = 0; j < SCREEN_WIDTH; j++) {
      Block block = field[i][j];
      uint32_t encoded = encode_block(block);

      // Split the encoded block into four separate uint8_t values
      uint8_t b1 = encoded & 0xFF;
      uint8_t b2 = (encoded >> 8) & 0xFF;
      uint8_t b3 = (encoded >> 16) & 0xFF;
      uint8_t b4 = (encoded >> 24) & 0xFF;

      // Store the uint8_t values in the EEPROM
      EEPROM.put((i * SCREEN_WIDTH * 4 + j * 4)+ address, b1);
      EEPROM.put((i * SCREEN_WIDTH * 4 + j * 4 + 1)+ address, b2);
      EEPROM.put((i * SCREEN_WIDTH * 4 + j * 4 + 2)+ address, b3);
      EEPROM.put((i * SCREEN_WIDTH * 4 + j * 4 + 3)+ address, b4);
    }
  }

  EEPROM.commit();
}

void loadGame()
{
  M5.Lcd.fillScreen(0);
  delete_matrix();
  EEPROM.get(0, Bombs_enabled);
  EEPROM.get(1, Timed_Virus_enabled);
  EEPROM.get(2, score);
  EEPROM.get(3, turn);
  EEPROM.get(4, x);
  EEPROM.get(5, y);
  EEPROM.get(6, x2);
  EEPROM.get(7, y2);
  EEPROM.get(8, countdown_active);
  EEPROM.get(9, countdown);
  EEPROM.get(10, virus_count);
  EEPROM.get(11, medecine_state);
  EEPROM.get(12, SCREEN_HEIGHT);
  EEPROM.get(13, SCREEN_WIDTH);
  SQUARE_WIDTH = M5.Lcd.width() / SCREEN_WIDTH;
  SQUARE_HEIGHT = M5.Lcd.height() / SCREEN_HEIGHT;
  EEPROM.get(14, color_1);
  EEPROM.get(15, color_2);
  EEPROM.get(16, next_color_1);
  EEPROM.get(17, next_color_2);
  EEPROM.get(18, virus_rarity);
  create_matrix();
  clear_field();
  int address = 19;
  uint8_t byte;
for (int i = 0; i < SCREEN_HEIGHT; i++) {
    for (int j = 0; j < SCREEN_WIDTH; j++) {
      // Load the four uint8_t values from the EEPROM
      uint8_t b1 = EEPROM.get(i * SCREEN_WIDTH * 4 + j * 4 + address, byte);
      uint8_t b2 = EEPROM.get(i * SCREEN_WIDTH * 4 + j * 4 + 1 + address, byte);
      uint8_t b3 = EEPROM.get(i * SCREEN_WIDTH * 4 + j * 4 + 2 + address, byte);
      uint8_t b4 = EEPROM.get(i * SCREEN_WIDTH * 4 + j * 4 + 3 + address, byte);

      // Combine the uint8_t values into a single uint32_t value
      uint32_t encoded = b1 | (b2 << 8) | (b3 << 16) | (b4 << 24);

      // Decode the uint32_t value and store it in the field
      field[i][j] = decode_block(encoded);
    }
  }
  EEPROM.commit();
}

void clear_field()
{
  for (int i = 0; i < SCREEN_HEIGHT; i++)
  {
    for (int j = 0; j < SCREEN_WIDTH; j++){
        field[i][j] = Empty_Block;
    }
  }
}
///////////////////////////////////////////
void init_field()
{
  virus_count = 0;
  for (int i = 0; i < SCREEN_HEIGHT; i++)
  {
    for (int j = 0; j < SCREEN_WIDTH; j++)
    {
      if (i < 9)
      {
        field[i][j] = Empty_Block;
      }
      else
      {
        uint8_t r = rand() % virus_rarity; // Generate a random number between 0 and 13
        if (r > 2)
        {
          field[i][j] = Empty_Block;
        }
        else
        {
          virus_count++;
          // Generate a random color
          field[i][j] = (Block){0, 0, 0, 2, r};
        }
      }
    }
  }
}

void delete_block(int j, int i)
{
  int type = field[i][j].type;
  if (type == 0 || type == 1 || type == 3)
    M5.Lcd.drawRect(SQUARE_WIDTH * j, SQUARE_HEIGHT * i, SQUARE_WIDTH, SQUARE_HEIGHT, BLACK);
  else if (type == 4)
    M5.Lcd.drawRoundRect(SQUARE_WIDTH * j, SQUARE_HEIGHT * i, SQUARE_WIDTH, SQUARE_HEIGHT, 10, BLACK);
  else if (type == 5)
    M5.Lcd.drawCircle((SQUARE_WIDTH * j) + SQUARE_WIDTH / 2, (SQUARE_HEIGHT * i) + SQUARE_HEIGHT / 2, (SQUARE_WIDTH / 2), BLACK);
  else if (type == 6 || type == 2)
    M5.Lcd.fillRect(SQUARE_WIDTH * j, SQUARE_HEIGHT * i, SQUARE_WIDTH, SQUARE_HEIGHT, BLACK);
}

void draw_field()
{
  for (int i = 0; i < SCREEN_HEIGHT; i++)
  {
    for (int j = 0; j < SCREEN_WIDTH; j++)
    {
      int type = field[i][j].type;
      uint32_t color = which_color(field[i][j].color, type);
      
      if (type == 0 || type == 1 || type == 3) // 0 and 1 for medecin, 3 for empty_block
        M5.Lcd.drawRect(SQUARE_WIDTH * j, SQUARE_HEIGHT * i, SQUARE_WIDTH, SQUARE_HEIGHT, color);
      else if (type == 2 || type == 6)//2 for virus, 6 timed virus
        M5.Lcd.fillRect(SQUARE_WIDTH * j, SQUARE_HEIGHT * i, SQUARE_WIDTH, SQUARE_HEIGHT, color);
      else if (type == 4)
        M5.Lcd.drawRoundRect(SQUARE_WIDTH * j, SQUARE_HEIGHT * i, SQUARE_WIDTH, SQUARE_HEIGHT, 10, color);
      else if (type == 5)
        M5.Lcd.drawCircle((SQUARE_WIDTH * j) + SQUARE_WIDTH / 2, (SQUARE_HEIGHT * i) + SQUARE_HEIGHT / 2, (SQUARE_WIDTH / 2), color);
    }
  }
}

void new_medecin()
{
  if (field[4][SCREEN_WIDTH / 2].type != 3)
  {
    game_over = true;
  }
  else
  {
    turn++;
    countdown--;
    medecine_state = 1;
    x = SCREEN_WIDTH / 2;
    y = 3;
    x2 = SCREEN_WIDTH / 2;
    y2 = 2;
    color_1 = next_color_1;
    color_2 = next_color_2;
    next_color_1 = rand() % 3;
    next_color_2 = rand() % 3;
    field[1][SCREEN_WIDTH / 2].color = next_color_1;
    field[0][SCREEN_WIDTH / 2].color = next_color_2;
    draw_field();
  }
}
/// TIMED VIRUS/////////////////////////////////
void init_timed_virus()
{
  if (Timed_Virus_enabled)
  {
    int r = rand() % virus_count;
    int index = 0;
    for (int i = 7; i < SCREEN_HEIGHT; i++)
    {
      for (int j = 0; j < SCREEN_WIDTH; j++)
      {
        if (field[i][j].type == 2)
        {
          if (index == r)
          {
            // Turn the virus block into a timed_virus
            delete_block(j, i);
            int color = field[i][j].color;
            field[i][j] = Timed_Virus;
            field[i][j].color = color;
            countdown = 10;
            countdown_active = true;
          }
          index++;
        }
      }
    }
  }
  else
    countdown_active = false;
}

void update_countdown()
{
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.setTextSize(2);
  M5.Lcd.printf("%i", virus_count);
  if (countdown_active)
  {
    M5.Lcd.setCursor(M5.lcd.width() - 20, 0);
    M5.Lcd.setTextSize(2);
    M5.Lcd.printf("%i", countdown);
  }
}
/// BOMB/////////////////////////////////
void explode(int x, int y)
{
  for (int i = -1; i <= 1; i++)
  {
    for (int j = -1; j <= 1; j++)
    {
      int p_x = field[y + i][x + j].pair_x;
      int p_y = field[y + i][x + j].pair_y;
      if (x + j >= 0 && x + j < SCREEN_WIDTH && y + i >= 0 && y + i < SCREEN_HEIGHT)
      {
        delete_block(x + j, y + i);
        if (field[y + i][x + j].type == 2)
        {
          virus_count--;
        }
        if (field[y + i][x + j].type == 4)
        {
          score += 4;
        }
        if (field[y + i][x + j].type == 6)
        {
          countdown_active = false;
          virus_count--;
        }
        delete_block(p_x, p_y);
        field[p_y][p_x].type = 4;   // now it is able to fall because it is no longer attached to its pair
        field[p_y][p_x].pair_x = 0; // since it no longer has a pair i change to pos of its partner to 0,0 since there is nothing there.
        field[p_y][p_x].pair_y = 0;
        field[y + i][x + j] = Empty_Block;
      }
    }
  }
  drop_medecin();
  drop_medecin();
}

int bomb_spawn_timer = 0;
int bomb_spawn_time = rand() % 300;

void spawn_bomb()
{
  if (bomb_spawn_timer > bomb_spawn_time && Bombs_enabled)
  {
    bomb_spawn_timer = 0;
    bomb_spawn_time = rand() % 300;
    // Count the number of medecin blocks
    int count = 0;
    for (int i = 5; i < SCREEN_HEIGHT; i++)
    {
      for (int j = 0; j < SCREEN_WIDTH; j++)
      {
        if (field[i][j].type == 0 && i != y && j != x)
        {
          count++;
        }
      }
    }
    // Pick a random medecin block
    if (count > 0)
    {
      int r = rand() % count;
      int index = 0;
      for (int i = 5; i < SCREEN_HEIGHT; i++)
      {
        for (int j = 0; j < SCREEN_WIDTH; j++)
        {
          if (field[i][j].type == 0 && i != y && j != x)
          {
            if (index == r)
            {
              // Turn the medecin block into a bomb
              int pair_x = field[i][j].pair_x;
              int pair_y = field[i][j].pair_y;
              field[pair_y][pair_x] = Empty_Block;
              delete_block(j, i);
              field[i][j] = Bomb;
              int y = i;
              while ((y < SCREEN_HEIGHT - 1) && (field[y + 1][j].type == 3))
              { // type 3 is empty block
                y++;
              }
              // Swap the broken medecin with an empty block at the bottom
              Block temp = field[y][j];
              delete_block(j, y);
              field[y][j] = field[i][j];
              delete_block(j, i);
              field[i][j] = temp;
              draw_field();
              delay(300);
              explode(j, y);
            }
            index++;
          }
        }
      }
    }
  }
  bomb_spawn_timer++;
}
////////////////////////////////////////
void next_level()
{
  virus_rarity -= virus_rarity/5;
  gravity_time -= gravity_time/10;
  delete_matrix();
  SCREEN_WIDTH = random(6, 17);
  SCREEN_HEIGHT = random(10, 17);
  field = new Block *[SCREEN_HEIGHT];
  SQUARE_WIDTH = M5.Lcd.width() / SCREEN_WIDTH;
  SQUARE_HEIGHT = M5.Lcd.height() / SCREEN_HEIGHT;
  setup();
}

bool end_game_called = false;
void end_game()
{
  if (end_game_called)
    return;
  else
  {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, M5.Lcd.height() / 2);
    M5.Lcd.setTextSize(2);
    if ((virus_count == 0))
    {
      M5.Lcd.print("NEXT LEVEL");
      M5.Lcd.setCursor(0, (M5.Lcd.height() / 2) + SQUARE_HEIGHT + 5);
      M5.Lcd.setTextSize(2);
      M5.Lcd.printf("SCORE: %i", score);
      delay(2000);
      next_level();
    }
    if (game_over)
    {
      end_game_called = true;
      delete_matrix();
      M5.Lcd.print("GAME OVER");
      M5.Lcd.setCursor(0, (M5.Lcd.height() / 2) + SQUARE_HEIGHT + 5);
      M5.Lcd.setTextSize(2);
      M5.Lcd.printf("SCORE: %i", score);
    }
  }
}

////////////////MENU////////////////////
const int NUM_OPTIONS = 4;
const char *options[NUM_OPTIONS] = {"B: OFF V: OFF", "B: OFF V: ON", "B: ON V: OFF", "B: ON V: ON"};
bool menu_called = false;

int selected_option = 0;

void Menu()
{
  while (true)
  {
    M5.Lcd.fillScreen(0);                     
    M5.Lcd.setCursor(0, M5.Lcd.height() / 2); 
    M5.Lcd.setTextSize(2);                    
    M5.Lcd.println("Menu:");                
    M5.Lcd.setTextSize(0);      
    for (int i = 0; i < NUM_OPTIONS; i++)
    {
      if (i == selected_option)
      {
        M5.Lcd.print("> "); 
      }
      M5.Lcd.println(options[i]);
    }
    M5.update();
    if (M5.BtnB.wasPressed())
    {
      menu_called = true;
      break; 
    }
    if (M5.BtnA.wasPressed())
    {
      selected_option = (selected_option + 1) % NUM_OPTIONS;
    }
  }
  if (selected_option == 0)
  {
    Bombs_enabled = false;
    Timed_Virus_enabled = false;
  }
  else if (selected_option == 1)
  {
    Bombs_enabled = false;
    Timed_Virus_enabled = true;
  }
  else if (selected_option == 2)
  {
    Bombs_enabled = true;
    Timed_Virus_enabled = false;
  }
  else if (selected_option == 3)
  {
    Bombs_enabled = true;
    Timed_Virus_enabled = true;
  }
}

///////////////////////////////////////
void setup()
{
  M5.begin();
  M5.IMU.Init();
  Serial.begin(115200);
  Serial.flush();
  EEPROM.begin(512);
  if (!menu_called)
    Menu();
  create_matrix();
  M5.Lcd.fillScreen(BLACK);
  init_field();
  init_timed_virus();
  new_medecin();
  draw_field();
}

void drop_medecin()
{
  for (int loop = 0; loop < 2; loop++)
  {
    for (int i = 2; i < SCREEN_HEIGHT; i++)
    {
      for (int j = 0; j < SCREEN_WIDTH; j++)
      {
        if (field[i][j].type == 4)
        {
          int y = i;
          while ((y < SCREEN_HEIGHT - 1) && (field[y + 1][j].type == 3))
          { // type 3 is empty block
            y++;
          }
          // Swap the broken medecin with an empty block at the bottom
          Block temp = field[y][j];
          delete_block(j, y);
          field[y][j] = field[i][j];
          delete_block(j, i);
          field[i][j] = temp;
        }
        else if (field[i][j].type == 0 || field[i][j].type == 1)
        {
          // Find the other block in the pair
          int y, x, y2, x2;
          int state = field[i][j].end_state;
          if (field[i][j].type == 0)
          {
            y2 = field[i][j].pair_y;
            x2 = field[i][j].pair_x;
            y = i;
            x = j;
          }
          else
          {
            y2 = i;
            x2 = j;
            y = field[i][j].pair_y;
            x = field[i][j].pair_x;
          }
          int ctr = 1;
          if ((state == 1 || state == 3) && valid_move('d', y, x, y2, x2, state, true) == 1)
          {
            while (valid_move('d', y + ctr, x, y2 + ctr, x2, state, true) == 1)
            {
              ctr++;
            }
            if (((y + ctr < SCREEN_HEIGHT - 1) && state == 1 && field[(y + ctr)][x].type == 3) || (y2 + ctr < SCREEN_HEIGHT - 1) && (state == 3 && field[(y2 + ctr)][x2].type == 3))
            {
              // Move the blocks down
              Block temp2 = field[y2][x2];
              Block temp = field[y][x];
              delete_block(x2, y2);
              delete_block(x, y);
              field[y][x] = Empty_Block;
              field[y2][x2] = Empty_Block;

              field[y + ctr][x] = temp;
              field[y + ctr][x].pair_y = y2+ctr;
              field[y2 + ctr][x2] = temp2;
              field[y2 + ctr][x].pair_y = y+ctr;
            }
          }
          else if ((state == 2 || state == 4) && valid_move('d', y , x, y2, x2, state, true) == 1)
          {
            while (valid_move('d', y + ctr, x, y2 + ctr, x2, state, true) == 1)
            {
              ctr++;
            }
            if (((y + ctr < SCREEN_HEIGHT - 1) && field[(y + ctr)][x].type == 3) && ((y2 + ctr < SCREEN_HEIGHT - 1) && field[(y2 + ctr)][x2].type == 3))
            {
              // Move the blocks down
              Block temp2 = field[y2][x2];
              Block temp = field[y][x];
              delete_block(x2, y2);
              delete_block(x, y);
              field[y][x] = Empty_Block;
              field[y2][x2] = Empty_Block;

              field[y + ctr][x] = temp;
              field[y + ctr][x].pair_y = y2+ctr;
              field[y2 + ctr][x2] = temp2;
              field[y2 + ctr][x].pair_y = y+ctr;
            }
          }
        }
      }
    }
  }
  four_in_a_row();
}
////////////////////////////////////////
void four_in_a_row()
{
  // Check rows
  for (int i = 2; i < SCREEN_HEIGHT; i++)
  {
    // Set the initial color to the first block in the row
    int current_color = field[i][0].color;
    int count = 1;
    // Iterate through the rest of the blocks in the row
    for (int j = 1; j < SCREEN_WIDTH; j++)
    {
      if (field[i][j].type == 3) // Skip blocks of type 3 (empty blocks)
      {
        continue;
      }
      // If the current block has the same color as the previous block, increment the count
      if (field[i][j].color == current_color)
      {
        count++;
      }
      else
      {
        // If the current block has a different color, reset the count and update the current color
        current_color = field[i][j].color;
        count = 1;
      }
      // If the count reaches four or more, turn the blocks into Blackcolor
      if (count >= 4)
      {
        for (int k = j - count + 1; k <= j; k++)
        {
          score++;
          int p_x = field[i][k].pair_x;
          int p_y = field[i][k].pair_y;
          if (field[i][k].type == 2)
          {
            virus_count--;
          }
          if (field[i][k].type == 4)
          {
            score += 4;
          }
          if (field[i][k].type == 6)
          {
            virus_count--;
            countdown_active = false;
          }
          delete_block(p_x, p_y);
          field[p_y][p_x].type = 4;   // now it is able to fall because it is no longer attached to its pair
          field[p_y][p_x].pair_x = 0; // since it no longer has a pair i change to pos of its partner to 0,0 since there is nothing there.
          field[p_y][p_x].pair_y = 0;
          delete_block(k, i);
          field[i][k] = Empty_Block;
        }
        drop_medecin();
      }
    }
  }
  // Check columns
  for (int j = 0; j < SCREEN_WIDTH; j++)
  {
    // Set the initial color to the first block in the column
    int current_color = field[2][j].color;
    int count = 1;
    // Iterate through the rest of the blocks in the column
    for (int i = 3; i < SCREEN_HEIGHT; i++)
    {
      if (field[i][j].type == 3) // Skip blocks of type 3 (empty blocks)
      {
        continue;
      }
      // If the current block has the same color as the previous block, increment the count
      if (field[i][j].color == current_color)
      {
        count++;
      }
      else
      {
        // If the current block has a different color, reset the count and update the current color
        current_color = field[i][j].color;
        count = 1;
      }
      // If the count reaches four or more, turn the blocks into Blackcolor
      if (count >= 4)
      {
        for (int k = i - count + 1; k <= i; k++)
        {
          score++;
          int p_x = field[k][j].pair_x;
          int p_y = field[k][j].pair_y;
          if (field[k][j].type == 2)
          {
            virus_count--;
          }
          if (field[k][j].type == 4)
          {
            score += 4;
          }
          if (field[k][j].type == 6)
          {
            virus_count--;
            countdown_active = false;
          }
          delete_block(p_x, p_y);
          field[p_y][p_x].type = 4;   // now it is able to fall because it is no longer attached to its pair
          field[p_y][p_x].pair_x = 0; // since it no longer has a pair i change to pos of its partner to 0,0 since there is nothing there.
          field[p_y][p_x].pair_y = 0;
          delete_block(j, k);
          field[k][j] = Empty_Block;
        }
        drop_medecin();
      }
    }
  }
}
//////////////////ROTATE////////////////
int good_rotation()
{
  if (x == SCREEN_WIDTH - 1 || x2 == SCREEN_WIDTH - 1 || x == 0 || x2 == 0 || y == SCREEN_HEIGHT - 1 || y2 == SCREEN_HEIGHT - 1) // can't turn if you are at the edge
    return 0;
  else if (medecine_state == 1)
  {
    if (field[y][x - 1].type == 3)
      return 1;
    else
      return 0;
  }
  else if (medecine_state == 2)
  {
    if (field[y - 1][x].type == 3)
      return 1;
    else
      return 0;
  }
  else if (medecine_state == 3)
  {
    if (field[y][x + 1].type == 3)
      return 1;
    else
      return 0;
  }
  else if (medecine_state == 4)
  {
    if (field[y + 1][x - 1].type == 3)
      return 1;
    else
      return 0;
  }
}

void rotate()
{
  if (M5.BtnA.isPressed())
  {
    int can_rotate = good_rotation();
    if (medecine_state == 1 && good_rotation() == 1)
    {
      medecine_state++;
      field[y][x] = Empty_Block;
      field[y2][x2] = Empty_Block;
      x -= 1;
      y2 += 1;
      field[y][x].color = color_1;
      field[y2][x2].color = color_2;
    }
    else if (medecine_state == 2 && good_rotation() == 1)
    {
      medecine_state++;
      field[y][x] = Empty_Block;
      field[y2][x2] = Empty_Block;
      y -= 1;
      x2 -= 1;
      field[y][x].color = color_1;
      field[y2][x2].color = color_2;
    }
    else if (medecine_state == 3 && good_rotation() == 1)
    {
      medecine_state++;
      field[y][x] = Empty_Block;
      field[y2][x2] = Empty_Block;
      x += 1;
      y2 -= 1;
      field[y][x].color = color_1;
      field[y2][x2].color = color_2;
    }
    else if (medecine_state == 4 && good_rotation() == 1)
    {
      medecine_state = 1;
      field[y][x] = Empty_Block;
      field[y2][x2] = Empty_Block;
      x -= 1;
      y += 1;
      field[y][x].color = color_1;
      field[y2][x2].color = color_2;
    }
  }
}
///////////////////////////////////////
void save_end_state()
{ // This will save the last state of a medicin in the struct, the pairs position, it's type and color
  field[y][x] = (Block){medecine_state, x2, y2, 0, color_1};
  field[y2][x2] = (Block){medecine_state, x, y, 1, color_2};
}
///////////////////////////////////////
int valid_move(char direction, int y, int x, int y2, int x2, int med_state, bool drop_med)
{
  if (direction == 'd')
  {
    if (y == SCREEN_HEIGHT - 1 || y2 == SCREEN_HEIGHT - 1)
    {
      if (!drop_med)
      {
        save_end_state();
        four_in_a_row();
        drop_medecin();
        new_medecin();
      }
      return 0;
    }
    else if(y > SCREEN_HEIGHT - 1 || y2 > SCREEN_HEIGHT - 1)
    return 0;
    else if (field[(y + 1)][x].type == 3 && med_state == 1)
      return 1;
    else if (field[(y2 + 1)][x2].type == 3 && med_state == 3)
      return 1;
    else if ((field[(y2 + 1)][x2].type == 3 && field[(y + 1)][x].type == 3) && (med_state == 2 || med_state == 4))
      return 1;
    else
    {
      if (!drop_med)
      {
        save_end_state();
        four_in_a_row();
        drop_medecin();
        new_medecin();
      }
      return 0;
    }
  }
  else if (direction == 'u')
  {
    return 0;
  }
  else if (direction == 'r')
  {
    if (x == SCREEN_WIDTH - 1 || x2 == SCREEN_WIDTH - 1)
      return 0;
    else if (field[y2][(x2 + 1)].type == 3 && med_state == 2)
      return 1;
    else if (field[y][(x + 1)].type == 3 && med_state == 4)
      return 1;
    else if ((field[y2][(x2 + 1)].type == 3 && field[y][(x + 1)].type == 3) && (med_state == 1 || med_state == 3))
      return 1;
    else
      return 0;
  }
  else if (direction == 'l')
  {
    if (x == 0 || x2 == 0)
      return 0;
    else if (field[y][(x - 1)].type == 3 && med_state == 2)
      return 1;
    else if (field[y2][(x2 - 1)].type == 3 && med_state == 4)
      return 1;
    else if ((field[y2][(x2 - 1)].type == 3 && field[y][(x - 1)].type == 3) && (med_state == 1 || med_state == 3))
      return 1;
    else
      return 0;
  }
  return 0;
}
/////////////GRAVITY////////////
int gravity_timer = 0;

void gravity()
{
  if ((gravity_timer > gravity_time) && (valid_move('d', y, x, y2, x2, medecine_state, false) == 1))
  {
    field[y][x] = Empty_Block;
    field[y2][x2] = Empty_Block;
    y += 1;
    y2 += 1;
    field[y][x].color = color_1;
    field[y2][x2].color = color_2;
    gravity_timer = 0;
  }
  gravity_timer += 1;
}
//////////////Accelerometer///////////
int accelero_timer = 0;
int accelero_time = 1;
#define MIN_TILT 0.15

void accelero_move(float acc_x, float acc_y)
{
  if (accelero_timer > accelero_time)
  {
    accelero_timer = 0;
    if (acc_x > MIN_TILT && valid_move('l', y, x, y2, x2, medecine_state, false) == 1)
    {
      field[y][x] = Empty_Block;
      field[y2][x2] = Empty_Block;
      x -= 1;
      x2 -= 1;
      field[y][x].color = color_1;
      field[y2][x2].color = color_2;
    }
    else if (acc_x < -MIN_TILT && valid_move('r', y, x, y2, x2, medecine_state, false) == 1)
    {
      field[y][x] = Empty_Block;
      field[y2][x2] = Empty_Block;
      x += 1;
      x2 += 1;
      field[y][x].color = color_1;
      field[y2][x2].color = color_2;
    }
    else if (acc_y > MIN_TILT && valid_move('d', y, x, y2, x2, medecine_state, false) == 1)
    {
      field[y][x] = Empty_Block;
      field[y2][x2] = Empty_Block;
      y += 1;
      y2 += 1;
      field[y][x].color = color_1;
      field[y2][x2].color = color_2;
    }
  }
  accelero_timer++;
}
///////////////////////////////////////////////////////////
void loop()
{
  M5.update();
  if (M5.BtnA.wasPressed() && M5.BtnB.wasPressed())
  {
    loadGame();
  }
  if (M5.BtnB.wasPressed())
  {
    saveGame();
  }
  if (!game_over && (virus_count != 0))
  {
    if (0 == countdown && countdown_active)
      game_over = true;
    update_countdown();
    float acc_x = 0, acc_y = 0, acc_z = 0;
    M5.IMU.getAccelData(&acc_x, &acc_y, &acc_z);
    accelero_move(acc_x, acc_y);
    gravity();
    rotate();
    spawn_bomb();
    delay(200);
    draw_field();
  }
  else
    end_game();
}