#include <hardware/i2c.h>
#include <pico/i2c_slave.h>
#include <pico/stdlib.h>
#include <stdio.h>
#include <string.h>
#include "hardware/vreg.h"
#include "hardware/clocks.h"

#define NOW() to_us_since_boot(get_absolute_time())
#define LOG(start) printf("%llu\n", NOW() - start);
#define LOGD(size, diff) printf("%i, %llu\n", diff, size)
#define LOGD_H() printf("size, micros\n");

#define I2C_SLAVE_ADDR 0x17

// #define I2C_BAUDRATE 100000 // 100 kHz
// #define I2C_BAUDRATE 400000 // 400 kHz
#define I2C_BAUDRATE 1000000 // 1 MHz
// #define I2C_BAUDRATE 3500000 // 3.5 MHz - max tested stable

// For this example, we run both the master and slave from the same board.
// You'll need to wire pin GP4 to GP6 (SDA), and pin GP5 to GP7 (SCL).
#define I2C_SLAVE_SDA_PIN 4
#define I2C_SLAVE_SCL_PIN 5
#define I2C_MASTER_SDA_PIN 6
#define I2C_MASTER_SCL_PIN 7

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

struct I2CData
{
	GamepadState state;
	uint64_t timestamp = 0;
};

// Our handler is called from the I2C ISR, so it must complete quickly. Blocking calls /
// printing to stdio may interfere with interrupt handling.
static void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event) {
	static uint8_t buf[256] = {0};
	static int32_t receivedIndex = 0;

	switch (event) {
		case I2C_SLAVE_RECEIVE:
			buf[receivedIndex] = i2c_read_byte_raw(i2c);
			receivedIndex++;
			break;
		case I2C_SLAVE_REQUEST:
			// TODO: Do something useful?!?
			break;
		case I2C_SLAVE_FINISH:
			{
				I2CData *data = reinterpret_cast<I2CData *>(buf);
				uint64_t diff = NOW() - data->timestamp;
				LOGD(diff, receivedIndex);
				receivedIndex = 0;
				memcpy(buf, 0, 256);
			}
			break;
		default:
			break;
	}
}

void setup_slave() {
	gpio_init(I2C_SLAVE_SDA_PIN);
	gpio_set_function(I2C_SLAVE_SDA_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(I2C_SLAVE_SDA_PIN);

	gpio_init(I2C_SLAVE_SCL_PIN);
	gpio_set_function(I2C_SLAVE_SCL_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(I2C_SLAVE_SCL_PIN);

	i2c_init(i2c0, I2C_BAUDRATE);
	i2c_slave_init(i2c0, I2C_SLAVE_ADDR, &i2c_slave_handler);
}

void setup_master() {
	gpio_init(I2C_MASTER_SDA_PIN);
	gpio_set_function(I2C_MASTER_SDA_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(I2C_MASTER_SDA_PIN);

	gpio_init(I2C_MASTER_SCL_PIN);
	gpio_set_function(I2C_MASTER_SCL_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(I2C_MASTER_SCL_PIN);

	i2c_init(i2c1, I2C_BAUDRATE);
}

void loop_master() {
	LOGD_H();
	I2CData data;

	for (uint16_t i = 0; ; ++i) {
		data.state.buttons = i;
		data.timestamp = NOW();
		i2c_write_blocking(i2c1, I2C_SLAVE_ADDR, (uint8_t *)&data, sizeof(I2CData), false);

		sleep_ms(1000);
	}
}

int main() {
	stdio_init_all();
	sleep_ms(3000);

	setup_slave();
	setup_master();

	sleep_ms(2000);
	printf("I2C Master/Slave example\n");
	printf("I2C rate set to %u KHz\n", I2C_BAUDRATE / 1000);
	printf("Starting I2C transfer\n");
	sleep_ms(1000);

	loop_master();
}
