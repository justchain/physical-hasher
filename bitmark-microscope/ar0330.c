// Copyright Bitmark Inc. 2015-2015

#include <cyu3system.h>
#include <cyu3types.h>
#include <cyu3os.h>
#include <cyu3dma.h>
#include <cyu3error.h>
#include <cyu3uart.h>
#include <cyu3i2c.h>
#include <cyu3spi.h>
#include <cyu3gpio.h>
#include <cyu3utils.h>
#include <cyu3mipicsi.h>

#include "ar0330.h"
#include "gpio.h"
#include "focus.h"
#include "sensor_i2c.h"

#define DEBUG_ENABLE 0

// test mode -- uncomment only one for testing
//           -- comment all for normal operation
//#define TEST_MODE TEST_MODE_IDENTIFY
//#define TEST_MODE TEST_MODE_RED
//#define TEST_MODE TEST_MODE_GREEN_RED
//#define TEST_MODE TEST_MODE_BLUE
//#define TEST_MODE TEST_MODE_GREEN_BLUE
//#define TEST_MODE TEST_MODE_GREEN_ALL
//#define TEST_MODE TEST_MODE_COLOUR_BARS


// set pixel resolution: 8, 10 or 12
// lanes: 2 or 4
#define USE_N_BITS 12
#define USE_N_LANES 2

static const sensor_data_t AR0330_PrimaryInitialisation[] = {
	// from datasheet power-up sequence
	{0x3152, 0xA114},               // ?
	{0x304a, 0x0070},               // ?
	{SENSOR_DELAY, 1000},           // should equate to 150,000 EXTCLK periods

	// reset
	{0x301A, 0x0059},               // reset_register = Reset
	{SENSOR_DELAY, 100},
	{0x301A, 0x0050},               // reset_register = Unlock
	{SENSOR_DELAY, 100},

	// VCO input = 24MHz
	//    8 bit => 392MHz
	//   10 bit => 490MHz
	//   12 bit => 588MHz
#if USE_N_BITS == 8
	{0x302E, 3},                    // pre_pll_clk_div ( 8 bit, 392MHz)
	{0x3030, 49},                   // pll_multiplier  ( 8 bit, 392MHz)
#elif USE_N_BITS == 10
	{0x302E, 12},                   // pre_pll_clk_div (10 bit, 490MHz)
	{0x3030, 245},                  // pll_multiplier  (10 bit, 490MHz)
#elif USE_N_BITS == 12
	{0x302E, 2},                    // pre_pll_clk_div (12 bit, 588MHz)
	{0x3030, 49},                   // pll_multiplier  (12 bit, 588MHz)
#else
#error "only 10 or 12 bit modes supported"
#endif

#if USE_N_LANES == 2
	{0x31AE,0x0202},                // two lanes
#elif USE_N_LANES == 4
	{0x31AE,0x0204},                // four lanes
#else
#error "only 2 or 4 lanes supported"
#endif


#if USE_N_BITS == 8
	// PLL
#if USE_N_LANES == 2
	{0x302C, 2},                    // vt_sys_clk_div
	{0x302A, 4},                    // vt_pix_clk_div
	{0x3038, 1},                    // op_sys_clk_div
	{0x3036, 8},                    // op_pix_clk_div
#elif USE_N_LANES == 4
	{0x302C, 1},                    // vt_sys_clk_div
	{0x302A, 4},                    // vt_pix_clk_div
	{0x3038, 1},                    // op_sys_clk_div
	{0x3036, 8},                    // op_pix_clk_div
#else
#error "only 2 or 4 lanes supported"
#endif

	// 8 bit MIPI config
	{0x31AC, 0x0808},               // data_format_bits
	{0x31B0, 40},                   // frame_preamble
	{0X31B2, 14},                   // line_preamble
	{0X31B4, 0x5f77},               // MIPI_timing_0
	{0X31B6, 0x5299},               // MIPI_timing_1
	{0X31B8, 0x408e},               // MIPI_timing_2
	{0X31BA, 0x030c},               // MIPI_timing_3
	{0X31BC, 0x800a},               // MIPI_timing_4
	{0x31BE, 0x2003},               // MIPI_Config_status
	{0x31D0, 0x0000},               // compression = off

#elif USE_N_BITS == 10
	// PLL
#if USE_N_LANES == 2
	{0x302C, 2},                    // vt_sys_clk_div
	{0x302A, 5},                    // vt_pix_clk_div
	{0x3038, 1},                    // op_sys_clk_div
	{0x3036, 10},                   // op_pix_clk_div
#elif USE_N_LANES == 4
	{0x302C, 1},                    // vt_sys_clk_div
	{0x302A, 5},                    // vt_pix_clk_div
	{0x3038, 1},                    // op_sys_clk_div
	{0x3036, 10},                   // op_pix_clk_div
#else
#error "only 2 or 4 lanes supported"
#endif

	// 10 bit MIPI config
	{0x31AC, 0x0A0A},               // data_format_bits
	{0x31B0, 40},                   // frame_preamble
	{0X31B2, 14},                   // line_preamble
	{0X31B4, 0x2743},               // MIPI_timing_0
	{0X31B6, 0x114e},               // MIPI_timing_1
	{0X31B8, 0x2049},               // MIPI_timing_2
	{0X31BA, 0x0186},               // MIPI_timing_3
	{0X31BC, 0x8005},               // MIPI_timing_4
	{0x31BE, 0x2003},               // MIPI_Config_status
	{0x31D0, 0x0000},               // compression = off

#elif USE_N_BITS == 12
	// PLL
#if USE_N_LANES == 2
	{0x302C, 2},                    // vt_sys_clk_div
	{0x302A, 6},                    // vt_pix_clk_div
	{0x3038, 1},                    // op_sys_clk_div
	{0x3036, 12},                   // op_pix_clk_div
#elif USE_N_LANES == 4
	{0x302C, 1},                    // vt_sys_clk_div
	{0x302A, 6},                    // vt_pix_clk_div
	{0x3038, 1},                    // op_sys_clk_div
	{0x3036, 12},                   // op_pix_clk_div
#else
#error "only 2 or 4 lanes supported"
#endif

	// 12 bit MIPI config
	{0x31AC, 0x0C0C},               // data_format_bits
	{0x31B0, 36},                   // frame_preamble
	{0X31B2, 12},                   // line_preamble
	{0X31B4, 0x2643},               // MIPI_timing_0
	{0X31B6, 0x114e},               // MIPI_timing_1
	{0X31B8, 0x2048},               // MIPI_timing_2
	{0X31BA, 0x0186},               // MIPI_timing_3
	{0X31BC, 0x8005},               // MIPI_timing_4
	{0x31BE, 0x2003},               // MIPI_Config_status
	{0x31D0, 0x0000},               // compression = off

#else
#error "only 10 or 12 bit modes supported"
#endif


// IMPORTANT NOTE:
//
//   I did not see thin in the data sheet, but determined it experimentally.
//
//   Pixel order if the vertical start is:
//
//     even: GRGR..., bgbg...
//     odd:  bgbg..., RGRG...
//
// The capture is coded for GRGR..., bgbg...  so keep starts even.

//
// VGA settings
//
// 640x480 @ 60Hz
// NOTE: values MUST be even
#define X_VGA_RG 838
#define Y_VGA_RG 534
	{0x3004, X_VGA_RG},             // x address start  - this is red pixel
	{0x3008, X_VGA_RG + 640 - 1},   // x address end
	{0x3002, Y_VGA_RG},             // y address start  - this is red/green line
	{0x3006, Y_VGA_RG + 480 - 1},   // y address end

	{0x30A2,      1},               // x odd inc (No Skip)
	{0x30A6,      1},               // y odd inc (No Skip)

	{0x300C,    640 +  6},          // the number of pixel clock periods in one line time
	{0x300A,    480 + 12},          // the number of complete lines(rows) in the frame timing

	{0x3014,      0},               // fine integration time
	{0x3012,   1349},               // coarse integration time
	{0x3042,      0},               // extra delay

	// [cb] VGA

	{0x308A, X_VGA_RG},             // [cb] x address start  - this is red pixel
	{0x308E, X_VGA_RG + 640 - 1},   // [cb] x address end
	{0x308C, Y_VGA_RG},             // [cb] y address start  - this is red/green line
	{0x3090, Y_VGA_RG + 480 - 1},   // [cb] y address end

	{0x30AE,      1},               // [cb] x odd inc (No Skip)
	{0x30A8,      1},               // [cb] y odd inc (No Skip)

	{0x303E,    640 +  6},          // [cb] the number of pixel clock periods in one line time
	{0x30AA,    480 + 12},          // [cb] the number of complete lines(rows) in the frame timing

	{0x3018,      0},               // [cb] fine integration time
	{0x3016,   1349},               // [cb] coarse integration time


	//{0x3012, 0x0304},               // coarse_integration_time = 1210
	//{0x3016, 0x0304},               // coarse_integration_time_cb = 1554

	// general configuration
	{0x3040, 0x0000},               // read_mode = 0
	{0x3042, 0x0130},               // extra_delay = 85

	{0x3028, 0x0010},               // row_speed
#if 0
	{0x3064, 0x1902},               // smia_test (embedded data)
#else
	{0x3064, 0x1802},               // smia_test (no embedded data)
#endif

	{0x301E, 0x00a8},               // data_pedestal

	// gains
	{0x3060, 0x0000},               // analog_gain = 1x
	{0x30B0, 0x8000},               // digital_test = context A
	{0x305E, 0x0080},               // global_gain    = 1x
	{0x30C4, 0x0080},               // global_gain_cb = 1x
	{0x30BA, 0x000C},               // digital_ctrl = no dither

	{SENSOR_DELAY, 5},
};


//
// 720p setting
//
// 1280x720 @ 60 fps
// NOTE: values MUST be even
#define X_720_RG 518
#define Y_720_RG 418
static const sensor_data_t sensor_720p_regs[] = {
	{0x3004, X_720_RG},             // x address start  - this is red pixel
	{0x3008, X_720_RG + 1280 - 1},  // x address end
	{0x3002, Y_720_RG},             // y address start  - this is red/green line
	{0x3006, Y_720_RG + 720 - 1},   // y address end

	{0x30A2,      1},               // x odd inc (No Skip)
	{0x30A6,      1},               // y odd inc (No Skip)

	{0x300C,   1280 +  6},          // the number of pixel clock periods in one line time
	{0x300A,    720 + 12},          // the number of complete lines(rows) in the frame timing

	{0x3014,      0},               // fine integration time
	{0x3012,   1349},               // coarse integration time
	{0x3042,      0},               // extra delay
};

//
// 1080p settings
//
// 1920x1080 @ 30 fps EIS
// NOTE: values MUST be even
#define X_1080_RG 198
#define Y_1080_RG 238
static const sensor_data_t sensor_1080p_regs[] = {
	{0x3004, X_1080_RG},            // x address start  - this is red pixel
	{0x3008, X_1080_RG + 1920 - 1}, // x address end
	{0x3002, Y_1080_RG},            // y address start  - this is red/green line
	{0x3006, Y_1080_RG + 1080 - 1}, // y address end

	{0x30A2,      1},               // x odd inc (No Skip)
	{0x30A6,      1},               // y odd inc (No Skip)

	{0x300C,   1920 +  6},          // the number of pixel clock periods in one line time
	{0x300A,   1080 + 12},          // the number of complete lines(rows) in the frame timing

	{0x3014,      0},               // fine integration time
	{0x3012,   1349},               // coarse integration time
	{0x3042,      0},               // extra delay
};


//
// 5M pixel settings
//
// 2304 X 1296
// NOTE: values MUST be even
#define X_5M_RG 6
#define Y_5M_RG 120
static const sensor_data_t sensor_5MP_regs[] = {
	{0x3004, X_5M_RG},              // x address start  - this is red pixel
	{0x3008, X_5M_RG + 2304 - 1},   // x address end
	{0x3002, Y_5M_RG},              // y address start  - this is red/green line
	{0x3006, Y_5M_RG + 1296 - 1},   // y address end

	{0x30A2,      1},               // x odd inc (No Skip)
	{0x30A6,      1},               // y odd inc (No Skip)

	{0x300c,   2304 +  6},          // the number of pixel clock periods in one line time
	{0x300A,   1296 + 12},          // the number of complete lines(rows) in the frame timing

	{0x3014,      0},               // fine integration time
	{0x3012,   1349},               // coarse integration time
	{0x3042,      0},               // extra delay
};


// analog gain table for R0x3060 [5..0]
static uint8_t analog_gain[] = {
	(0 << 4) |  0,   //  0.00 dB
	(0 << 4) |  1,   //  0.26 dB
	(0 << 4) |  2,   //  0.56 dB
	(0 << 4) |  3,   //  0.86 dB
	(0 << 4) |  4,   //  1.16 dB
	(0 << 4) |  5,   //  1.46 dB
	(0 << 4) |  6,   //  1.80 dB
	(0 << 4) |  7,   //  2.14 dB
	(0 << 4) |  8,   //  2.50 dB
	(0 << 4) |  9,   //  2.87 dB
	(0 << 4) | 10,   //  3.25 dB
	(0 << 4) | 11,   //  3.66 dB
	(0 << 4) | 12,   //  4.08 dB
	(0 << 4) | 13,   //  4.53 dB
	(0 << 4) | 14,   //  5.00 dB
	(0 << 4) | 15,   //  5.49 dB
	(1 << 4) |  0,   //  6.00 dB
	(1 << 4) |  2,   //  6.58 dB
	(1 << 4) |  4,   //  7.18 dB
	(1 << 4) |  6,   //  7.82 dB
	(1 << 4) |  8,   //  8.52 dB
	(1 << 4) | 10,   //  9.28 dB
	(1 << 4) | 12,   // 10.10 dB
	(1 << 4) | 14,   // 11.02 dB
	(2 << 4) |  0,   // 12.00 dB
	(2 << 4) |  4,   // 13.20 dB
	(2 << 4) |  8,   // 14.54 dB
	(2 << 4) | 12,   // 16.12 dB
	(3 << 4) |  0    // 18.00 dB
};

void AR0330_Debug(void)
{
#if DEBUG_ENABLE
	uint16_t tempaddr = 0x3000;
	int i;
	for (i = 0; i < 256; i += 4) {
		CyU3PDebugPrint(4, "DEBUG: 0x%x:  ", tempaddr);
		int j;
		for (j = 0; j < 4; ++j, tempaddr += 2) {
			uint16_t d;
			sensor_read(tempaddr, &d);
			CyU3PDebugPrint(4, "0x%x%x%x%x ",
					 (d >> 12) & 0x0f,
					 (d >>  8) & 0x0f,
					 (d >>  4) & 0x0f,
					 (d >>  0) & 0x0f);
		}
		CyU3PDebugPrint(4, "\r\n");
	}

	static const uint16_t addresses[] = {
		0x3780,
		0x301A  // reset status
//		0x3ED0,  0x3ED2,  0x3ED4,  0x3ED6,
//		0x3ED8,  0x3EDA,  0x3EDC,  0x3EDE,
//		0x3EE0,                    0x3EE6,
//		0x3EE8,  0x3EEA,
//		0x3F06,
	};

	for (i = 0; i < SIZE_OF_ARRAY(addresses); ++i) {
		tempaddr = addresses[i];
		uint16_t tempdata;
		sensor_read(tempaddr, &tempdata);
		CyU3PDebugPrint(4, "DEBUG: read address = 0x%x data = 0x%x\r\n", tempaddr, tempdata);
	}

	// report if MIPI is active
	CyU3PDebugPrint(4, "DEBUG: MIPI active = %d\r\n", CyU3PMipicsiCheckBlockActive());
#endif
}

#if 0
static CyBool_t p_switch = CyFalse;
static int p_n = 0;
static int p_i = 1;
static CyBool_t check_photo_switch(void) {
	p_n += p_i;
	CyBool_t value = CyFalse;
	CyU3PReturnStatus_t rc = CyU3PGpioSimpleGetValue(FOCUS_POSITION, &value);
	CyBool_t state = (CY_U3P_SUCCESS == rc) && (CyTrue == value);
	if (state != p_switch) {
		p_switch = state;
		CyU3PDebugPrint(4, "ps = %x @ %d\r\n", state, p_n);
	}
	return state;
}

static int s_index = 0;
static void step(int direction) {

	const uint8_t full_fwd_step[] = {
		//0355, 0345, 0344, 0354 // low
		0333, 0332, 0322, 0323 // medium
		//0311, 0301, 0300, 0310, // high
	};


	s_index += direction;
	if (s_index < 0) {
		s_index = sizeof(full_fwd_step) - 1;
	} else if (s_index >= sizeof(full_fwd_step)) {
		s_index = 0;
	}
	CyU3PSpiTransmitWords((uint8_t *)&full_fwd_step[s_index], 1);
	CyU3PThreadSleep(10);
}
#endif

void AR0330_Base_Config(void) {
	uint16_t tempdata;
	uint16_t tempaddr;

	CyU3PDebugPrint(4, "AR0330_Base_Config: starting...\r\n");

	//SensorReset(); // already done elsewhere


	// GPIO configuration

	CyU3PGpioSimpleConfig_t anInput = {
		.outValue = CyFalse,
		.driveLowEn = CyFalse,
		.driveHighEn = CyFalse,
		.inputEn = CyTrue,
		.intrMode = CY_U3P_GPIO_NO_INTR
	};

	CyU3PGpioSimpleConfig_t anOutputInitiallyLow = {
		.outValue = CyFalse,
		.driveLowEn = CyTrue,
		.driveHighEn = CyTrue,
		.inputEn = CyFalse,
		.intrMode = CY_U3P_GPIO_NO_INTR
	};

	CyU3PGpioSimpleConfig_t anOutputInitiallyHigh = {
		.outValue = CyFalse,
		.driveLowEn = CyTrue,
		.driveHighEn = CyTrue,
		.inputEn = CyFalse,
		.intrMode = CY_U3P_GPIO_NO_INTR
	};

	struct {
		int pin;
		CyU3PGpioSimpleConfig_t *config;
	} initialiseIO[] = GPIO_SETUP_BLOCK;

	for (int i = 0; NULL != initialiseIO[i].config; ++i) {
		CyU3PReturnStatus_t status = CyU3PDeviceGpioOverride(initialiseIO[i].pin, CyTrue);
		if (CY_U3P_SUCCESS != status) {
			CyU3PDebugPrint(4, "AR0330_Base_Config: override pin: %d error = %d 0x%x\r\n", initialiseIO[i].pin, status, status);
			continue;
		}

		status = CyU3PGpioSetSimpleConfig(initialiseIO[i].pin, initialiseIO[i].config);
		if (CY_U3P_ERROR_NOT_CONFIGURED == status) {
			CyU3PDebugPrint(4, "AR0330_Base_Config: pin: %d  not configured as simple IO in matrix\r\n", initialiseIO[i].pin);
		} else if (CY_U3P_SUCCESS != status) {
			CyU3PDebugPrint(4, "AR0330_Base_Config: pin: %d error = %d 0x%x\r\n", initialiseIO[i].pin, status, status);
		}
	}

	// set up focus system
	Focus_Initialise();

	// sensor configuration
	SENSOR_WRITE_ARRAY(AR0330_PrimaryInitialisation);

	// display versions
	tempaddr = 0x3000;
	sensor_read(tempaddr, &tempdata);
	CyU3PDebugPrint(4, "AR0330_Base_Config: chip version@0x%x = 0x%x\r\n", tempaddr, tempdata);

	tempaddr = 0x300E;
	sensor_read(tempaddr, &tempdata);
	CyU3PDebugPrint(4, "AR0330_Base_Config: revision number@0x%x = 0x%x\r\n", tempaddr, tempdata);

	// report if MIPI is active
	CyU3PDebugPrint(4, "AR0330_Base_Config: MIPI active = %d\r\n", CyU3PMipicsiCheckBlockActive());
}


void AR0330_VGA_config(void) {
	CyU3PDebugPrint(4, "AR0330_VGA_config\r\n");
}


void AR0330_VGA_HS_config(void) {
	CyU3PDebugPrint(4, "AR0330_VGA_HS_config\r\n");

	AR0330_Debug();
}


void AR0330_720P_config(void) {
	CyU3PDebugPrint(4, "AR0330_720P_config\r\n");

	//SENSOR_WRITE_ARRAY(AR0330_PrimaryInitialisation);
	SENSOR_WRITE_ARRAY(sensor_720p_regs);

	AR0330_Debug();
}

void AR0330_1080P_config(void) {
	CyU3PDebugPrint(4, "AR0330_1080P_config\r\n");

	//SENSOR_WRITE_ARRAY(AR0330_PrimaryInitialisation);
	SENSOR_WRITE_ARRAY(sensor_1080p_regs);

	AR0330_Debug();
}


void AR0330_5MP_config(void) {
	CyU3PDebugPrint(4, "AR0330_5MP_config\r\n");

	//SENSOR_WRITE_ARRAY(AR0330_PrimaryInitialisation);
	SENSOR_WRITE_ARRAY(sensor_5MP_regs);

	AR0330_Debug();
}


void AR0330_Power_Down(void) {
	CyU3PDebugPrint(4, "AR0330_Power_Down\r\n");

	//sensor_write(0x301a, 0x0058);
	sensor_write(0x301a, 0x0050);
}


// possible test modes
#define TEST_MODE_IDENTIFY     1
#define TEST_MODE_RED          2
#define TEST_MODE_GREEN_RED    3
#define TEST_MODE_BLUE         4
#define TEST_MODE_GREEN_BLUE   5
#define TEST_MODE_GREEN_ALL    6
#define TEST_MODE_COLOUR_BARS  7

void AR0330_Power_Up(void) {
	CyU3PDebugPrint(4, "AR0330_Power_Up\r\n");

	// set test mode
#if defined(TEST_MODE)
	sensor_write(0x301E, 0x00a8);            // data_pedestal
	sensor_write(0x3070, 0x0001);            // set test mode

#define pedestal 0x80
#define full (0x0fff - pedestal)
#define low  (0x001f + pedestal)

#if TEST_MODE == TEST_MODE_IDENTIFY
	sensor_write(0x3072, 0x01aa + pedestal); // red
	sensor_write(0x3074, 0x02bb + pedestal); // green (in red row)
	sensor_write(0x3076, 0x03cc + pedestal); // blue
	sensor_write(0x3078, 0x04dd + pedestal); // green (in blue row)

#elif TEST_MODE == TEST_MODE_RED
	sensor_write(0x3072, full);              // red
	sensor_write(0x3074, low);               // green (in red row)
	sensor_write(0x3076, low);               // blue
	sensor_write(0x3078, low);               // green (in blue row)

#elif TEST_MODE == TEST_MODE_GREEN_RED
	sensor_write(0x3072, low);               // red
	sensor_write(0x3074, full);              // green (in red row)
	sensor_write(0x3076, low);               // blue
	sensor_write(0x3078, low);               // green (in blue row)

#elif TEST_MODE == TEST_MODE_BLUE
	sensor_write(0x3072, low);               // red
	sensor_write(0x3074, low);               // green (in red row)
	sensor_write(0x3076, full);              // blue
	sensor_write(0x3078, low);               // green (in blue row)

#elif TEST_MODE == TEST_MODE_GREEN_BLUE
	sensor_write(0x3072, low);               // red
	sensor_write(0x3074, low);               // green (in red row)
	sensor_write(0x3076, low);               // blue
	sensor_write(0x3078, full);              // green (in blue row)

#elif TEST_MODE == TEST_MODE_GREEN_ALL
	sensor_write(0x3072, low);               // red
	sensor_write(0x3074, full);              // green (in red row)
	sensor_write(0x3076, low);               // blue
	sensor_write(0x3078, full);              // green (in blue row)

#elif TEST_MODE == TEST_MODE_COLOUR_BARS
	sensor_write(0x3070, 0x0002);            // set test mode

#else
#error "undefined test mode"
#endif

#else
	// normal mode
	sensor_write(0x3070, 0x0000);            // disable test mode
#endif

	// start streaming
	//sensor_write(0x301a, 0x005c);
	sensor_write(0x301a, 0x0054);

	CyU3PThreadSleep(10);

	AR0330_Debug();
}


void AR0330_Auto_Focus_Config(void) {
	CyU3PDebugPrint(4, "AR0330_Auto_Focus_Config\r\n");
}


#define BRIGHTNESS_RESOLUTION   1
#define BRIGHTNESS_MINIMUM      0
#define BRIGHTNESS_MAXIMUM 0x0fff
#define BRIGHTNESS_DEFAULT 0x00a8

static int32_t current_brightness = BRIGHTNESS_DEFAULT;

void AR0330_SetBrightness(int32_t brightness) {
	CyU3PDebugPrint(4, "AR0330_SetBrightness: %d\r\n", brightness);
	if (brightness < BRIGHTNESS_MINIMUM && brightness > BRIGHTNESS_MAXIMUM) {
		return; // ignore invalid values
	}
	current_brightness = brightness;
	sensor_write(0x301e, (uint16_t)(brightness));  // data_pedestal
}

int32_t AR0330_GetBrightness(int32_t option) {
	CyU3PDebugPrint(4, "AR0330_GetBrightness\r\n");
	switch (option) {
	default:
		return 0;
	case 1:  // current value
		return current_brightness;
	case 2:  // minimum value
		return BRIGHTNESS_MINIMUM;
	case 3:  // maximum value
		return BRIGHTNESS_MAXIMUM;
	case 4:  // resolution
		return BRIGHTNESS_RESOLUTION;
	case 7:  // default
		return BRIGHTNESS_DEFAULT;
	}
}


#define CONTRAST_RESOLUTION  1
#define CONTRAST_MINIMUM     0
#define CONTRAST_MAXIMUM     SIZE_OF_ARRAY(analog_gain) - 1
#define CONTRAST_DEFAULT     0

static int32_t current_contrast = CONTRAST_DEFAULT;

void AR0330_SetContrast(int32_t contrast) {
	CyU3PDebugPrint(4, "AR0330_SetContrast %d\r\n", contrast);
	if (contrast < CONTRAST_MINIMUM && contrast > CONTRAST_MAXIMUM) {
		return; // ignore invalid values
	}
	current_contrast = contrast;
	uint16_t gain = analog_gain[contrast] & 0x3f;
	// 6 bit fields: cb=13..8  ca=5..0
	sensor_write(0x3060, (gain << 8) | gain);  // analog_gain_cb | analog_gain
}


int32_t AR0330_GetContrast(int32_t option) {
	CyU3PDebugPrint(4, "AR0330_GetContrast\r\n");

	switch (option) {
	default:
		return 0;
	case 1:  // current value
		return current_contrast;
	case 2:  // minimum value
		return 0;
	case 3:  // maximum value
		return SIZE_OF_ARRAY(analog_gain) - 1;
	case 4:  // resolution
		return 1;
	case 7:  // default
		return 0;
	}
}


#define SHARPNESS_RESOLUTION   1
#define SHARPNESS_MINIMUM      0
#define SHARPNESS_MAXIMUM 0x07ff
#define SHARPNESS_DEFAULT 0x0080

static int32_t current_sharpness = SHARPNESS_DEFAULT;

void AR0330_SetSharpness(int32_t sharpness) {
	CyU3PDebugPrint(4, "AR0330_SetSharpness: %d\r\n", sharpness);
	if (sharpness < SHARPNESS_MINIMUM && sharpness > SHARPNESS_MAXIMUM) {
		return; // ignore invalid values
	}
	current_sharpness = sharpness;
	sensor_write(0x305e, (uint16_t)(sharpness));  // global_gain
}

int32_t AR0330_GetSharpness(int32_t option) {
	CyU3PDebugPrint(4, "AR0330_GetSharpness\r\n");
	switch (option) {
	default:
		return 0;
	case 1:  // current value
		return current_sharpness;
	case 2:  // minimum value
		return SHARPNESS_MINIMUM;
	case 3:  // maximum value
		return SHARPNESS_MAXIMUM;
	case 4:  // resolution
		return SHARPNESS_RESOLUTION;
	case 7:  // default
		return SHARPNESS_DEFAULT;
	}
}


#define HUE_RESOLUTION   1
#define HUE_MINIMUM      0
#define HUE_MAXIMUM 0x00ff
#define HUE_DEFAULT 0x0000

static int32_t current_hue = HUE_DEFAULT;


void AR0330_SetHue(int32_t hue) {
	CyU3PDebugPrint(4, "AR0330_SetHue: %d\r\n", hue);

#define SetLow(io) CyU3PGpioSetValue(io, CyFalse)
#define SetHigh(io) CyU3PGpioSetValue(io, CyTrue)
#define SetBit(io, value) CyU3PGpioSetValue(io, 0 != (value & 0x80))

#if 0
	CyU3PReturnStatus_t status;
	status = CyU3PGpioSetValue(LED_DRIVER_SDI, 0 != (hue & 1));
	if (CY_U3P_SUCCESS != status) {
		CyU3PDebugPrint(4, "AR0330_SetHue: error = %d 0x%x\r\n", status, status);
	}
	status = CyU3PGpioSetValue(LED_DRIVER_CLK, 0 != (hue & 2));
	if (CY_U3P_SUCCESS != status) {
		CyU3PDebugPrint(4, "AR0330_SetHue: error = %d 0x%x\r\n", status, status);
	}
	status = CyU3PGpioSetValue(LED_DRIVER_ED1, 0 != (hue & 4));
	if (CY_U3P_SUCCESS != status) {
		CyU3PDebugPrint(4, "AR0330_SetHue: error = %d 0x%x\r\n", status, status);
	}
	status = CyU3PGpioSetValue(LED_DRIVER_ED2, 0 != (hue & 8));
	if (CY_U3P_SUCCESS != status) {
		CyU3PDebugPrint(4, "AR0330_SetHue: error = %d 0x%x\r\n", status, status);
	}

#else
	// initialise
	SetLow(LED_DRIVER_CLK);
	SetHigh(LED_DRIVER_ED2);
	SetLow(LED_DRIVER_ED1);
	CyU3PThreadSleep(1);

	for (int i = 0; i < 8; ++i) {
		SetBit(LED_DRIVER_SDI, hue);
		hue <<= 1; // output bit big endian
		CyU3PBusyWait(10);
		//CyU3PThreadSleep(10);
		SetHigh(LED_DRIVER_CLK);
		CyU3PBusyWait(10);
		//CyU3PThreadSleep(10);
		SetLow(LED_DRIVER_CLK);
	}
	//CyU3PThreadSleep(1);
	CyU3PBusyWait(10);
	SetHigh(LED_DRIVER_ED1);
	//CyU3PThreadSleep(10);
	CyU3PBusyWait(10);
	SetLow(LED_DRIVER_ED1);
	//CyU3PThreadSleep(1);
	CyU3PBusyWait(10);
	SetLow(LED_DRIVER_ED2);
#endif
}

int32_t AR0330_GetHue(int32_t option) {
	CyU3PDebugPrint(4, "AR0330_GetHue\r\n");
	switch (option) {
	default:
		return 0;
	case 1:  // current value
		return current_hue;
	case 2:  // minimum value
		return HUE_MINIMUM;
	case 3:  // maximum value
		return HUE_MAXIMUM;
	case 4:  // resolution
		return HUE_RESOLUTION;
	case 7:  // default
		return HUE_DEFAULT;
	}
}


void AR0330_SetSaturation(int32_t saturation)
{
	CyU3PDebugPrint(4, "AR0330_SetSaturation: %d\r\n", saturation);
}

int32_t AR0330_GetSaturation(int32_t option) {
	CyU3PDebugPrint(4, "AR0330_GetSaturation\r\n");
	return 0;
}


void AR0330_SetWhiteBalance(int32_t white_balance) {
	CyU3PDebugPrint(4, "AR0330_SetWhiteBalance: %d\r\n", white_balance);
}

int32_t AR0330_GetWhiteBalance(int32_t option) {
	CyU3PDebugPrint(4, "AR0330_GetWhiteBalance\r\n");
	return 0;
}


void AR0330_SetAutoWhiteBalance(int32_t AutoWhiteBalance) {
	CyU3PDebugPrint(4, "AR0330_SetAutoWhiteBalance: %s\r\n", AutoWhiteBalance ? "true" : "false");
}


int32_t AR0330_GetAutoWhiteBalance(int32_t option) {
	CyU3PDebugPrint(4, "AR0330_GetAutoWhiteBalance\r\n");
	return 0;
}


void AR0330_SetExposure(int32_t Exposure) {
	CyU3PDebugPrint(4, "AR0330_SetExposure\r\n");
}


int32_t AR0330_GetExposure(int32_t option) {
	CyU3PDebugPrint(4, "AR0330_GetExposure\r\n");
#if 0
	CyBool_t clr_error_count = CyFalse;
	CyU3PMipicsiErrorCounts_t error_count;

	CyU3PMipicsiGetErrors(clr_error_count, &error_count);
	CyU3PDebugPrint(4, "Error count %d\r\n", clr_error_count);

	CyU3PDebugPrint(4, "frmErrCnt =  %d\r\n", error_count.frmErrCnt);
	CyU3PDebugPrint(4, "crcErrCnt =  %d\r\n", error_count.crcErrCnt);
	CyU3PDebugPrint(4, "mdlErrCnt =  %d\r\n", error_count.mdlErrCnt);
	CyU3PDebugPrint(4, "ctlErrCnt =  %d\r\n", error_count.ctlErrCnt);

	CyU3PDebugPrint(4, "eidErrCnt =  %d\r\n", error_count.eidErrCnt);
	CyU3PDebugPrint(4, "recrErrCnt =  %d\r\n", error_count.recrErrCnt);
	CyU3PDebugPrint(4, "unrcErrCnt =  %d\r\n", error_count.unrcErrCnt);
	CyU3PDebugPrint(4, "recSyncErrCnt =  %d\r\n", error_count.recSyncErrCnt);
	CyU3PDebugPrint(4, "unrSyncErrCnt =  %d\r\n", error_count.unrSyncErrCnt);
#endif
	return 0;
}


void AR0330_SetAutofocus(int32_t enable) {
	CyU3PDebugPrint(4, "AR0330_SetAutofocus\r\n");
	if (0 != enable) {
		// trigger a new focus cycle
		Focus_Start();
	}
}


int32_t AR0330_GetAutofocus(int32_t option) {
	CyU3PDebugPrint(4, "AR0330_GetAutofocus\r\n");
	return 1;
}


void AR0330_SetManualfocus(int32_t enable) {
	CyU3PDebugPrint(4, "AR0330_SetManualfocus\r\n");
	if (0 != enable) {
		// stop auto focus process
		Focus_Stop();
	}
}


int32_t AR0330_GetManualfocus(int32_t option) {
	CyU3PDebugPrint(4, "AR0330_GetManualfocus\r\n");
	return 0;
}


void AR0330_SetAutoExposure(int32_t AutoExp) {
	CyU3PDebugPrint(4, "AR0330_SetAutoExposure\r\n");
}

int32_t AR0330_GetAutoExposure(int32_t option) {
	CyU3PDebugPrint(4, "AR0330_GetAutoExposure\r\n");
	return 0;
}
