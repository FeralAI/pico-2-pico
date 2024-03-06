#include <stdio.h>
#include <cstring>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/multicore.h"
#include "hardware/spi.h"
#include "hardware/clocks.h"

#define NOW() to_us_since_boot(get_absolute_time())
#define LOG(start) printf("%llu\n", NOW() - start);
#define LOGD(diff, size, data) printf("MICROS: %llu, BYTES: %i, DATA: %X\n", diff, size, data)

#define SPI_XFER_BITS 8
#define SPI_SPEED_HZ (1000 * 1000) // 1MHz
// #define SPI_SPEED_HZ (5000 * 1000) // 5MHz
// #define SPI_SPEED_HZ (20000 * 1000) // 10MHz
#define SPI_CPOL SPI_CPOL_0
#define SPI_CPHA SPI_CPHA_0

#define SPI_MASTER spi0
#define SPI_MASTER_RX 16
#define SPI_MASTER_CS 17
#define SPI_MASTER_SCK 18
#define SPI_MASTER_TX 19

#define SPI_SLAVE spi1
#define SPI_SLAVE_RX 12
#define SPI_SLAVE_CS 13
#define SPI_SLAVE_SCK 14
#define SPI_SLAVE_TX 15

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

struct SpiData
{
	GamepadState state;
	uint64_t timestamp = 0;
};

#define BUF_LEN 24

void printbuf(uint8_t buf[], size_t len) {
	size_t i;
	for (i = 0; i < len; ++i) {
		if (i % 16 == 15)
			printf("%02x\n", buf[i]);
		else
			printf("%02x ", buf[i]);
	}

	// append trailing newline if there isn't one
	if (i % 16) {
		putchar('\n');
	}
}

void setup_master() {
	spi_init(SPI_MASTER, SPI_SPEED_HZ);
    spi_set_format(SPI_MASTER, SPI_XFER_BITS, SPI_CPOL, SPI_CPHA, SPI_MSB_FIRST);
	gpio_set_function(SPI_MASTER_RX, GPIO_FUNC_SPI);
	gpio_set_function(SPI_MASTER_SCK, GPIO_FUNC_SPI);
	gpio_set_function(SPI_MASTER_TX, GPIO_FUNC_SPI);
	gpio_set_function(SPI_MASTER_CS, GPIO_FUNC_SPI);
	// gpio_pull_up(SPI_MASTER_CS);
	printf("Master setup complete\n");
}

void setup_slave() {
	spi_init(SPI_SLAVE, SPI_SPEED_HZ);
    spi_set_format(SPI_SLAVE, SPI_XFER_BITS, SPI_CPOL, SPI_CPHA, SPI_MSB_FIRST);
	spi_set_slave(SPI_SLAVE, true);
	gpio_set_function(SPI_SLAVE_RX, GPIO_FUNC_SPI);
	gpio_set_function(SPI_SLAVE_SCK, GPIO_FUNC_SPI);
	gpio_set_function(SPI_SLAVE_TX, GPIO_FUNC_SPI);
	gpio_set_function(SPI_SLAVE_CS, GPIO_FUNC_SPI);
	// gpio_pull_up(SPI_SLAVE_CS);
	printf("Slave setup complete\n");
}

void loop_master() {
	multicore_lockout_victim_init();

	SpiData data;
	uint64_t nextRunTime = 0;

	for (size_t i = 0; ; ++i) {
		while (!spi_is_writable(SPI_MASTER) || (int64_t)(nextRunTime - NOW()) > 0)
			tight_loop_contents();

		// data.state.buttons = i;
		data.timestamp = NOW();

		spi_write_blocking(SPI_MASTER, (uint8_t *)&data, BUF_LEN);

		// Write to stdio whatever came in on the MISO line.
		// printf("SPI master says: sent page %d from the MISO line:\n", i);
		// printbuf(out_buf, BUF_LEN);

		// Need to limit the send rate when master and slave are on the same device
		nextRunTime = NOW() + 100000ULL;
	}
}

void loop_slave() {
	for (int i = 0; ; ++i) {
		while (!spi_is_readable(SPI_SLAVE))
			tight_loop_contents();

		SpiData data;
		int size = spi_read_blocking(SPI_SLAVE, 0xFF, (uint8_t *)&data, BUF_LEN);
		uint64_t diff = NOW() - data.timestamp;
		printf("SPI slave says: read page %i from the MOSI line with size %i\n", i, size);
		LOG(data.timestamp);
		printbuf((uint8_t *)&data, BUF_LEN);
	}
}

int main() {
	stdio_init_all();

	setup_master();
	setup_slave();

	sleep_ms(2000);
	printf("SPI master example\n");
	printf("SPI rate set to %.2f MHz\n", spi_get_baudrate(SPI_MASTER) / 1000000.f);
	printf("Starting SPI transfer\n");
	sleep_ms(1000);

	multicore_launch_core1(loop_master);
	loop_slave();
}
