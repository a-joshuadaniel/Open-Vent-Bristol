#include "board/board.h"
#include "clock/clock.h"
#include "gpio/gpio.h"
#include "spi/spi.h"
#include "uart/uart.h"
#include "adc/adc.h"
#include "ftp_types.h"
#include "gpio/multiplexer.h"

void process_command(ftp_command_t* command);
extern void uart_ftp_get_command(ftp_command_t* command);
extern void spi_ftp_run(void);

static const char string_error_not_implemented[] = " - not implemented";
static const char string_high[] = "HIGH";
static const char string_low[] = "LOW";
static const char string_newline[] = "\r\n>";
static const char string_welcome[] = "\r\nOpenVent - FTP";

static volatile uint16_t g_timer_ms;

// Scheduler ISR interrupts at 1kHz
SCHEDULER_ISR()
{
  if (g_timer_ms != 0u)
  {
    g_timer_ms--;
  }
}

int main(void)
{
  g_timer_ms = 0u;

  clock_init();
  gpio_init();
  uart_init();
  adc_init();
  spi_init();
  spi_setup_transaction(&LATCH_PORT, LATCH_PIN);

  SCHEDULER_START();

  sei();

  uart_write(string_welcome, sizeof(string_welcome));
  uart_write(string_newline, sizeof(string_newline));

  ftp_command_t ftp_command = {0u};

  while (1u)
  {
    if (g_timer_ms == 0u)
    {
      // Make the spare GPIO high for the duration of processing - this enables the scheduler
      // frequency to be checked on a scope
      gpio_set_pin(&GPIO_SPARE_PORT, GPIO_SPARE_PIN);

      uart_ftp_get_command(&ftp_command);
      if (ftp_command.instruction != NONE)
      {
        process_command(&ftp_command);
      }

      // Clear the spare GPIO - for checking frequency
      gpio_clear_pin(&GPIO_SPARE_PORT, GPIO_SPARE_PIN);

      // Set timer so there's a delay before processing again
      g_timer_ms = 20u;
    }
  }
}

void process_command(ftp_command_t* command)
{
  ADC_resolution_t adc_reading = -1;
  uint8_t digital_reading = 0xFF;

  switch(command->instruction)
  {
    case READ_ANALOGUE_FLOW:
      adc_reading = adc_read_blocking(ADC_FLOW);
      break;

    case READ_ANALOGUE_PRESSURE:
      adc_reading = adc_read_blocking(ADC_PRESSURE);
      break;

    case READ_ANALOGUE_VBATT:
      adc_reading = adc_read_blocking(ADC_VBATT);
      break;

    case READ_ANALOGUE_MOTOR:
      adc_reading = adc_read_blocking(ADC_MOTOR_CURRENT);
      break;

    case READ_ANALOGUE_TEMP:
      adc_reading = adc_read_blocking(ADC_TEMP);
      break;

    case READ_ALERT_INTERLOCK:
      digital_reading = multiplexer_digital_read_channel(MUX_SELECT_CASE_INTERLOCK);
      break;

    case READ_ALERT_PRESSURE:
      digital_reading = multiplexer_digital_read_channel(MUX_SELECT_PRESSURE_ALERT);
      break;

    case READ_ALERT_FLOW:
      digital_reading = multiplexer_digital_read_channel(MUX_SELECT_FLOW_ALERT);
      break;

    case READ_ALERT_VBATT:
      digital_reading = multiplexer_digital_read_channel(MUX_SELECT_VBATT_ALERT);
      break;

    case READ_SWITCH_1:
      digital_reading = multiplexer_digital_read_channel(MUX_SELECT_MEMBRANE_SW_1);
      break;

    case READ_SWITCH_2:
      digital_reading = multiplexer_digital_read_channel(MUX_SELECT_MEMBRANE_SW_2);
      break;

    case READ_SWITCH_3:
      digital_reading = multiplexer_digital_read_channel(MUX_SELECT_MEMBRANE_SW_3);
      break;

    case READ_SWITCH_4:
      digital_reading = multiplexer_digital_read_channel(MUX_SELECT_MEMBRANE_SW_4);
      break;

    case RUN_MOTOR_IN:
      gpio_set_pin(&MOTOR_IN_A_PORT, MOTOR_IN_A_PIN);
      gpio_clear_pin(&MOTOR_IN_B_PORT, MOTOR_IN_B_PIN);
      MOTOR_PWM(command->arg);
      MOTOR_PWM_START();
      break;

    case RUN_MOTOR_OUT:
      gpio_clear_pin(&MOTOR_IN_A_PORT, MOTOR_IN_A_PIN);
      gpio_set_pin(&MOTOR_IN_B_PORT, MOTOR_IN_B_PIN);
      MOTOR_PWM(command->arg);
      MOTOR_PWM_START();
      break;

    case MOTOR_STOP:
      MOTOR_PWM_STOP();
      break;

    case PRINT_SPI:
      spi_ftp_run();
      uart_write("- check scope", 13u);
      break;

    case NONE:
      uart_write(string_error_not_implemented, sizeof(string_error_not_implemented));
      break;
  }

  if (digital_reading == 0u)
  {
    uart_write(string_newline, sizeof(string_newline));
    uart_write(string_low, sizeof(string_low));
  }
  else if (digital_reading == 1u)
  {
    uart_write(string_newline, sizeof(string_newline));
    uart_write(string_high, sizeof(string_high));
  }
  else if (adc_reading != -1)
  {
    // 5 spaces will work for anything up to a 16-bit ADC
    char mirror_str[5] = {'\0'};
    uint8_t i = 0u;
    while (adc_reading)
    {
      ADC_resolution_t tenth = adc_reading / 10u;
      mirror_str[i++] = adc_reading - (tenth * 10u) + '0';
      adc_reading = tenth;
    }

    uart_write(string_newline, sizeof(string_newline));

    // Characters are in reverse order!
    if (i == 0u)
    {
      char zero = '0';
      uart_write(&zero, 1u);
    }
    else
    {
      for (; i > 0u; i--)
      {
        uart_write(&mirror_str[i - 1u], 1u);
      }
    }
  }

  uart_write(string_newline, sizeof(string_newline));
}
