#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <aarch64/rpi_gpio.h>
#include <screen/screen.h>

#define PLAYER_WIDTH 80
#define PLAYER_HEIGHT 240
#define WINDOW_HEIGHT 480
#define WINDOW_WIDTH 800

#define GPIO_P1_L 26
#define GPIO_P1_R 17
#define GPIO_P1_1 27
#define GPIO_P1_2 22
#define GPIO_P2_L 18
#define GPIO_P2_R 25
#define GPIO_P2_1 5
#define GPIO_P2_2 6

#define MOVEMENT_SPEED 5
#define PLAYER_HEIGHT 240
#define WINDOW_HEIGHT 480
#define WINDOW_WIDTH 800

typedef struct {
  int x;
} Player;

Player p1 = {.x = 200};

Player p2 = {.x = 400};

void draw_player(Player *p, int *buffer, int stride) {
  int *lbuffer = buffer;
  int player_head = PLAYER_HEIGHT;
  lbuffer += (stride / 4) * (WINDOW_HEIGHT - player_head);
  for (int i = player_head; i > 0; i--, lbuffer += (stride / 4)) {
    for (int j = p->x; j < p->x + PLAYER_WIDTH; j++) {
      lbuffer[j] = 0xFFFFFFFF;
    }
  }
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
  rpi_gpio_set_select(GPIO_P2_L, RPI_GPIO_FUNC_IN);
  rpi_gpio_set_select(GPIO_P2_R, RPI_GPIO_FUNC_IN);
}

void handle_inputs() {
    fprintf(stdout, "%d\n",rpi_gpio_read(GPIO_P1_L) );
    if (rpi_gpio_read(GPIO_P1_L)) move_player_left(&p1);
    if (rpi_gpio_read(GPIO_P1_R)) move_player_right(&p1);
    if (rpi_gpio_read(GPIO_P2_L)) move_player_left(&p2);
    if (rpi_gpio_read(GPIO_P2_R)) move_player_right(&p2);
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
  draw_player(&p1, ptr, stride);
  draw_player(&p2, ptr, stride);

  err = screen_post_window(*screen_window, *screen_buf, 0, NULL, 0);
  if (err != 0) {
    printf("Failed to post window\n");
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

    setup_gpios();

  /* Trap execution */
  while (1) {
    render(&screen_buf, &screen_window);
    handle_inputs();
    delay(15);
  }

  screen_destroy_window(screen_window);
  screen_destroy_context(screen_context);

  return EXIT_SUCCESS;
}
