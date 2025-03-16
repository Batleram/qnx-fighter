#include <aarch64/rpi_gpio.h>
#include <math.h>
#include <pthread.h>
#include <rpi_ws281x.h>
#include <screen/screen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <time.h>
#include <unistd.h>

#define PLAYER_WIDTH 80
#define PLAYER_HEIGHT 240
#define WINDOW_HEIGHT 480
#define WINDOW_WIDTH 800

#define GPIO_P1_L 26
#define GPIO_P1_R 17
#define GPIO_P1_1 22
#define GPIO_P1_2 27
#define GPIO_P1_HP 10
#define GPIO_P2_L 18
#define GPIO_P2_R 25
#define GPIO_P2_1 5
#define GPIO_P2_2 6
#define GPIO_P2_HP 20

#define MOVEMENT_SPEED 5
#define ATTACK_RADIUS (PLAYER_HEIGHT + 100) / 2

#define PLAYER_HEIGHT 240
#define MAX_HP 3
#define WINDOW_HEIGHT 480
#define WINDOW_WIDTH 800

#define ATTACK_CD 800
#define HIT_CD 500
#define PARRY_CD 250
#define PARRY_COUNTER_CD 1000

#define ON_NORMAL_CD 1
#define ON_ATTACK_CD 2
#define ON_HIT_CD 3
#define ON_PARRY_CD 4

#define CIRCLE_COLOR 0xFFFF0000
#define NORMAL_COLOR 0xFFFFFFFF
#define ATTACK_COLOR 0xFF00FFFF
#define HIT_COLOR 0xFFFF0000
#define PARRY_COLOR 0xFF0000FF

#define GAME_CLOCK_DELAY 15

ws2811_t p1hpstrip = {
    .freq = WS2811_TARGET_FREQ,
    .channel =
        {
            [0] =
                {
                    .gpionum = GPIO_P1_HP,
                    .invert = 0,
                    .count = 6,
                    .strip_type = WS2811_STRIP_GRB,
                    .brightness = 255,
                },
            // no second channel
            [1] =
                {
                    .gpionum = -1,
                    .invert = 0,
                    .count = 0,
                    .brightness = 0,
                },
        },
};

ws2811_t p2hpstrip = {
    .freq = WS2811_TARGET_FREQ,
    .channel =
        {
            [0] =
                {
                    .gpionum = GPIO_P2_HP,
                    .invert = 0,
                    .count = 6,
                    .strip_type = WS2811_STRIP_GRB,
                    .brightness = 255,
                },
            // no second channel
            [1] =
                {
                    .gpionum = -1,
                    .invert = 0,
                    .count = 0,
                    .brightness = 0,
                },
        },
};

typedef struct {
  int x;
  int hitting;
  int hp;
  int hp_dirty;
  int state;
  int cooldown_ms;
} Player;

Player p1 = {.x = 200,
             .hitting = 0,
             .hp = MAX_HP,
             .hp_dirty = 1,
             .state = ON_NORMAL_CD,
             .cooldown_ms = 0};
Player p2 = {.x = 400,
             .hitting = 0,
             .hp = MAX_HP,
             .hp_dirty = 1,
             .state = ON_NORMAL_CD,
             .cooldown_ms = 0};

void draw_player(Player *p, int *buffer, int stride) {
  int color = 0x00;
  switch (p->state) {
  case ON_NORMAL_CD: {
    color = NORMAL_COLOR;
    break;
  }
  case ON_PARRY_CD: {
    color = PARRY_COLOR;
    break;
  }
  case ON_ATTACK_CD: {
    color = ATTACK_COLOR;
    break;
  }
  case ON_HIT_CD: {
    color = HIT_COLOR;
    break;
  }
  }

  int *lbuffer = buffer;
  int player_head = PLAYER_HEIGHT;
  lbuffer += (stride / 4) * (WINDOW_HEIGHT - player_head);
  for (int i = player_head; i > 0; i--, lbuffer += (stride / 4)) {
    for (int j = p->x; j < p->x + PLAYER_WIDTH; j++) {
      lbuffer[j] = color;
    }
  }
}

int point_distance_squared(int x1, int y1, int x2, int y2) {
  int dx;
  int dy;
  if (x1 > x2) {
    dx = x1 - x2;
  } else {
    dx = x2 - x1;
  }

  if (y1 > y2) {
    dy = y1 - y2;
  } else {
    dy = y2 - y1;
  }

  return dx * dx + dy * dy;
}

int in_range(int a, int b, int range) {
  if (a >= b && a - b <= range)
    return 1;
  if (a <= b && b - a <= range)
    return 1;
  return 0;
}

void draw_circle(int *buffer, int stride, int radius, int x, int y) {
  int radius_error = 200;
  int *lbuffer = buffer;
  int rr = radius * radius;
  for (int i = 0; i < WINDOW_HEIGHT; ++i, lbuffer += (stride / 4)) {
    for (int j = 0; j < WINDOW_WIDTH; j++) {
      if (in_range(point_distance_squared(x, y, j, i), rr, radius_error) == 1) {
        lbuffer[j] = CIRCLE_COLOR;
      }
    }
  }
}

void render_hit_radius(int *buffer, int stride, Player *p) {
  int y = WINDOW_HEIGHT - (PLAYER_HEIGHT / 2);
  int x = p->x + PLAYER_WIDTH / 2;

  draw_circle(buffer, stride, ATTACK_RADIUS, x, y);
}

void clear_screen(int *buffer, int stride) {
  int *lbuffer = buffer;
  for (int i = 0; i < WINDOW_HEIGHT; ++i, lbuffer += (stride / 4)) {
    for (int j = 0; j < WINDOW_WIDTH; j++) {
      lbuffer[j] = 0xFF000000;
    }
  }
}

int move_player_left(Player *p) {
  if (p->x <= MOVEMENT_SPEED) {
    p->x = MOVEMENT_SPEED;
    return 0;
  }

  p->x -= MOVEMENT_SPEED;

  return 1;
}

int move_player_right(Player *p) {
  if (p->x >= WINDOW_WIDTH - (PLAYER_WIDTH + MOVEMENT_SPEED)) {
    p->x = WINDOW_WIDTH - (PLAYER_WIDTH + MOVEMENT_SPEED);
    return 0;
  }

  p->x += MOVEMENT_SPEED;

  return 1;
}

// this name is hard-coded in 'rpi_gpio.h'
volatile uint32_t *rpi_gpio_regs;

void setup_gpios() {
  // required by 'rpi_gpio.h'
  rpi_gpio_regs = mmap(0, __PAGESIZE, PROT_READ | PROT_WRITE | PROT_NOCACHE,
                       MAP_SHARED | MAP_PHYS, -1, 0xfe200000);
  if (rpi_gpio_regs == MAP_FAILED)
    perror("pthread_create"), exit(EXIT_FAILURE);
  // setup inputs

  rpi_gpio_set_select(GPIO_P1_L, RPI_GPIO_FUNC_IN);
  rpi_gpio_set_select(GPIO_P1_R, RPI_GPIO_FUNC_IN);
  rpi_gpio_set_select(GPIO_P1_1, RPI_GPIO_FUNC_IN);
  rpi_gpio_set_select(GPIO_P1_2, RPI_GPIO_FUNC_IN);
  rpi_gpio_set_select(GPIO_P2_L, RPI_GPIO_FUNC_IN);
  rpi_gpio_set_select(GPIO_P2_R, RPI_GPIO_FUNC_IN);
  rpi_gpio_set_select(GPIO_P2_1, RPI_GPIO_FUNC_IN);
  rpi_gpio_set_select(GPIO_P2_2, RPI_GPIO_FUNC_IN);
}

int is_in_swing_range(Player *attacker, Player *defender) {
  int distance = 0;
  // if attacker is on the right of defender
  if (attacker->x >= defender->x) {
    int d = attacker->x - defender->x;
    // avoid negatives if they're in one-another
    if (d < PLAYER_WIDTH / 2)
      return 1;
    distance = d - PLAYER_WIDTH / 2;
  } else {
    int d = defender->x - attacker->x;
    if (d < PLAYER_WIDTH / 2)
      return 1;
    distance = d - PLAYER_WIDTH / 2;
  }

  if (distance < ATTACK_RADIUS)
    return 1;
  return 0;
}

void hitreg_player(Player *attacker, Player *defender) {
  if (attacker->cooldown_ms != 0)
    return;

  attacker->hitting = 1;
  attacker->cooldown_ms = ATTACK_CD;
  attacker->state = ON_ATTACK_CD;

  if (is_in_swing_range(attacker, defender) == 0) {
    return;
  }

  if (defender->state == ON_PARRY_CD) {
    attacker->state = ON_ATTACK_CD;
    attacker->cooldown_ms = PARRY_COUNTER_CD;
    return;
  }

  if (defender->state == ON_NORMAL_CD) {
    defender->hp--;
    defender->hp_dirty = 1;
    defender->state = ON_HIT_CD;
    defender->cooldown_ms = HIT_CD;
    return;
  }

  if (defender->state == ON_ATTACK_CD) {
    defender->hp--;
    defender->hp_dirty = 1;
    if (defender->cooldown_ms <= HIT_CD)
      defender->cooldown_ms = HIT_CD;
    defender->state = ON_HIT_CD;
    return;
  }
}

void handle_inputs() {
  if (rpi_gpio_read(GPIO_P1_L))
    move_player_left(&p1);
  if (rpi_gpio_read(GPIO_P1_R))
    move_player_right(&p1);
  if (rpi_gpio_read(GPIO_P2_L))
    move_player_left(&p2);
  if (rpi_gpio_read(GPIO_P2_R))
    move_player_right(&p2);

  if (p1.cooldown_ms == 0) {
    if (rpi_gpio_read(GPIO_P1_2)) {
      p1.cooldown_ms = PARRY_CD;
      p1.state = ON_PARRY_CD;
    }

    if (rpi_gpio_read(GPIO_P1_1) == 1) {
      hitreg_player(&p1, &p2);
    }
  }
  if (p2.cooldown_ms == 0) {
    if (rpi_gpio_read(GPIO_P2_2)) {
      p2.cooldown_ms = PARRY_CD;
      p2.state = ON_PARRY_CD;
    }

    if (rpi_gpio_read(GPIO_P2_1) == 1) {
      hitreg_player(&p2, &p1);
    }
  }
}

void render(screen_buffer_t *screen_buf, screen_window_t *screen_window) {
  int *ptr = NULL;
  int err = screen_get_buffer_property_pv(*screen_buf, SCREEN_PROPERTY_POINTER,
                                          (void **)&ptr);
  if (err != 0) {
    printf("Failed to get buffer pointer\n");
  }

  int stride = 0;
  err = screen_get_buffer_property_iv(*screen_buf, SCREEN_PROPERTY_STRIDE,
                                      &stride);
  if (err != 0) {
    printf("Failed to get buffer stride\n");
  }

  // clear screen
  memset(ptr, 0x00, WINDOW_HEIGHT * WINDOW_WIDTH * 4);
  clear_screen(ptr, stride);

  if (p1.hitting == 1) {
    render_hit_radius(ptr, stride, &p1);

    p1.hitting = 0;
  }
  if (p2.hitting == 1) {
    render_hit_radius(ptr, stride, &p2);
    p2.hitting = 0;
  }

  draw_player(&p1, ptr, stride);
  draw_player(&p2, ptr, stride);

  err = screen_post_window(*screen_window, *screen_buf, 0, NULL, 0);
  if (err != 0) {
    printf("Failed to post window\n");
  }
}

void *render_lights(void *args) {
  while (1) {
    if (p1.hp_dirty == 1) {
      for (int i = 0; i < 5; i++) {
        if (i < p1.hp) {
          p1hpstrip.channel[0].leds[i] = 0x00FF0000;
        } else {
          p1hpstrip.channel[0].leds[i] = 0x00000000;
        }
      }

      /*ws2811_render(&p1hpstrip);*/
      p1.hp_dirty = 0;
    }

    if (p2.hp_dirty == 1) {
      for (int i = 0; i < 5; i++) {
        if (i < p2.hp) {
          p2hpstrip.channel[0].leds[i] = 0x00FF0000;

        } else {
          p2hpstrip.channel[0].leds[i] = 0x00000000;
        }
      }

      /*ws2811_render(&p2hpstrip);*/
      p1.hp_dirty = 0;
    }

    delay(50);
  }
}

void tick(Player *p) {
  // for all intents and purposes, the game renders instantly, this will make it
  // much easier
  if (p->cooldown_ms <= GAME_CLOCK_DELAY) {
    p->cooldown_ms = 0;
    p->state = ON_NORMAL_CD;
  } else {
    p->cooldown_ms -= GAME_CLOCK_DELAY;
  }
}

int main(void) {
  int err = 0;
  screen_context_t screen_context = 0;
  screen_window_t screen_window = 0;

  err = screen_create_context(&screen_context, SCREEN_APPLICATION_CONTEXT);
  if (err != 0) {
    printf("Failed to create screen context\n");
  }

  err = screen_create_window(&screen_window, screen_context);
  if (err != 0) {
    printf("Failed to create screen window\n");
  }

  int usage = SCREEN_USAGE_READ | SCREEN_USAGE_WRITE;
  err = screen_set_window_property_iv(screen_window, SCREEN_PROPERTY_USAGE,
                                      &usage);
  if (err != 0) {
    printf("Failed to set usage property\n");
  }

  // set screen format to RGBA8888, 0xAARRGGBB
  int screenFormat = SCREEN_FORMAT_RGBA8888;
  screen_set_window_property_iv(screen_window, SCREEN_PROPERTY_FORMAT,
                                &screenFormat);
  if (err != 0) {
    printf("Failed to set format property\n");
  }

#if 1
  // set screen z ordor to 10, 0 is bottom, n(>0) is top
  int zorder = 15;
  screen_set_window_property_iv(screen_window, SCREEN_PROPERTY_ZORDER, &zorder);
  if (err != 0) {
    printf("Failed to set zorder property\n");
  }
#endif

  err = screen_create_window_buffers(screen_window, 1);
  if (err != 0) {
    printf("Failed to create window buffer\n");
  }

  int buffer[2];
  err = screen_get_window_property_iv(screen_window,
                                      SCREEN_PROPERTY_BUFFER_SIZE, buffer);
  if (err != 0) {
    printf("Failed to get window buffer size\n");
  }

  screen_buffer_t screen_buf;
  err = screen_get_window_property_pv(
      screen_window, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)&screen_buf);
  if (err != 0) {
    printf("Failed to get window buffer\n");
  }

#if defined(ENABLE_LED)
  // setup rgb strip memory area
  ws2811_init(&p1hpstrip);
  for (int i = 0; i < 5; i++)
    p1hpstrip.channel[0].leds[i] = 0x00000000;
  ws2811_init(&p2hpstrip);
  for (int i = 0; i < 5; i++)
    p2hpstrip.channel[0].leds[i] = 0x00000000;
#endif

  setup_gpios();

  // init threads
  nanospin_calibrate(1);

#if defined(ENABLE_LED)
  // launch `update` as a high priority thread
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

  int max_priority = sched_get_priority_max(SCHED_FIFO);
  struct sched_param sched = {.sched_priority = max_priority};
  pthread_attr_setschedparam(&attr, &sched);

  pthread_t thr_update;
  if (pthread_create(&thr_update, &attr, render_lights, NULL) == -1)
    perror("pthread_create"), exit(EXIT_FAILURE);
#endif

  /* Trap execution */
  while (1) {
    tick(&p1);
    tick(&p2);
    render(&screen_buf, &screen_window);
    handle_inputs();
    delay(GAME_CLOCK_DELAY);
  }

  screen_destroy_window(screen_window);
  screen_destroy_context(screen_context);

  return EXIT_SUCCESS;
}
