#include <hardware/i2c.h>
#include <pico/i2c_slave.h>
#include <pico/stdlib.h>
#include <stdio.h>
#include <string.h>
#include "hardware/vreg.h"
#include "hardware/clocks.h"

#define GP2040CE_GUEST_ID 0x17

// static const uint I2C_BAUDRATE = 100000; // 100 kHz
// static const uint I2C_BAUDRATE = 400000; // 400 kHz
// static const uint I2C_BAUDRATE = 1000000; // 1 MHz
static const uint I2C_BAUDRATE = 1500000; // 1.5 MHz

// For this example, we run both the master and slave from the same board.
// You'll need to wire pin GP4 to GP6 (SDA), and pin GP5 to GP7 (SCL).
static const uint I2C_SLAVE_SDA_PIN = PICO_DEFAULT_I2C_SDA_PIN; // 4
static const uint I2C_SLAVE_SCL_PIN = PICO_DEFAULT_I2C_SCL_PIN; // 5
static const uint I2C_MASTER_SDA_PIN = 6;
static const uint I2C_MASTER_SCL_PIN = 7;

#define GAMEPAD_JOYSTICK_MID 0x7FFF
struct GamepadState
{
	uint8_t dpad {0};
	uint16_t buttons {0};
	uint16_t aux {0};
	uint16_t lx {GAMEPAD_JOYSTICK_MID};
	uint16_t ly {GAMEPAD_JOYSTICK_MID};
	uint16_t rx {GAMEPAD_JOYSTICK_MID};
	uint16_t ry {GAMEPAD_JOYSTICK_MID};
	uint8_t lt {0};
	uint8_t rt {0};
};

static uint8_t buf[256] = {0};
static uint64_t micros = 0;

// Our handler is called from the I2C ISR, so it must complete quickly. Blocking calls /
// printing to stdio may interfere with interrupt handling.
static void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event) {
	static int32_t receivedIndex = -1;

	switch (event) {
		case I2C_SLAVE_RECEIVE: // master has written some data
			receivedIndex++;
			buf[receivedIndex] = i2c_read_byte_raw(i2c);
			break;
		case I2C_SLAVE_REQUEST: // master is requesting data load from memory
			// TODO: Do something useful?!?
			break;
		case I2C_SLAVE_FINISH: // master has signalled Stop / Restart
			{
				uint32_t diff = to_us_since_boot(get_absolute_time()) - micros;
				printf("MICROS: %i, BYTES: %i, DATA: %X\n", diff, sizeof(GamepadState), *reinterpret_cast<GamepadState *>(buf));
				receivedIndex = -1;
				memcpy(buf, 0, 256);
			}
			break;
		default:
			break;
	}
}

static void setup_slave() {
	gpio_init(I2C_SLAVE_SDA_PIN);
	gpio_set_function(I2C_SLAVE_SDA_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(I2C_SLAVE_SDA_PIN);

	gpio_init(I2C_SLAVE_SCL_PIN);
	gpio_set_function(I2C_SLAVE_SCL_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(I2C_SLAVE_SCL_PIN);

	i2c_init(i2c0, I2C_BAUDRATE);
	// configure I2C0 for slave mode
	i2c_slave_init(i2c0, GP2040CE_GUEST_ID, &i2c_slave_handler);
}

static void run_master() {
	gpio_init(I2C_MASTER_SDA_PIN);
	gpio_set_function(I2C_MASTER_SDA_PIN, GPIO_FUNC_I2C);
	// pull-ups are already active on slave side, this is just a fail-safe in case the wiring is faulty
	gpio_pull_up(I2C_MASTER_SDA_PIN);

	gpio_init(I2C_MASTER_SCL_PIN);
	gpio_set_function(I2C_MASTER_SCL_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(I2C_MASTER_SCL_PIN);

	i2c_init(i2c1, I2C_BAUDRATE);

	uint16_t i = 0;
	while (1) {
		GamepadState *state = new GamepadState {
			.dpad = 0,
			.buttons = i,
			.aux = 0,
			.lx = 0,
			.ly = 0,
			.rx = 0,
			.ry = 0,
			.lt = 0,
			.rt = 0,
		};

		// printf("%i %X\n", i, *state);

		micros = to_us_since_boot(get_absolute_time());
		i2c_write_blocking(i2c1, GP2040CE_GUEST_ID, (const uint8_t *)state, sizeof(GamepadState), false);
		i++;

		delete state;

		sleep_ms(1000);
	}
}

int main() {
	stdio_init_all();
	setup_slave();
	run_master();
	while (1);
}
