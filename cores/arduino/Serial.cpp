/*
  Serial.cpp - wrapper over mbed RawSerial
  Part of Arduino - http://www.arduino.cc/

  Copyright (c) 2018-2019 Arduino SA

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General
  Public License along with this library; if not, write to the
  Free Software Foundation, Inc., 59 Temple Place, Suite 330,
  Boston, MA  02111-1307  USA
*/

#include "Arduino.h"
#include "Serial.h"
#include "IRQManager.h"


#ifdef Serial
#undef Serial
#endif

static tx_buffer_index_t _tx_buffer_head[10];
static tx_buffer_index_t _tx_buffer_tail[10];
static rx_buffer_index_t _rx_buffer_tail[10];

static unsigned char *_rx_buffer[10];
static unsigned char *_tx_buffer[10];

static bool _sending[10];


uint8_t              tx_buff[SERIAL_TX_BUFFER_SIZE];
int                  to_send_i = -1;
int                  send_i = -1;



TxStatus_t tx_status = TX_STOPPED;

#define MAX_UARTS    10

static UART *_uarts[MAX_UARTS];


void uart_callback(uart_callback_args_t *p_args)
{
    /* This callback function is not used but it is referenced into 
       FSP configuration so that (for the moment it is necessary to keep it) */
}

/* -------------------------------------------------------------------------- */
void UART::WrapperCallback(uart_callback_args_t *p_args) {
/* -------------------------------------------------------------------------- */  

  uint32_t channel = p_args->channel;
  
    switch (p_args->event){
        case UART_EVENT_RX_COMPLETE:
        {
            R_SCI_UART_Read((uart_ctrl_t*)(SciTable[channel].uart_instance->p_ctrl), _rx_buffer[channel], SERIAL_RX_BUFFER_SIZE);
            break;
        }
        case UART_EVENT_ERR_PARITY:
        case UART_EVENT_ERR_FRAMING:
        case UART_EVENT_ERR_OVERFLOW:
        {
            break;
        }
        case UART_EVENT_TX_COMPLETE:
        {
          if(send_i != to_send_i) {
            inc(send_i, SERIAL_TX_BUFFER_SIZE);
            R_SCI_UART_Write ((uart_ctrl_t*)(SciTable[channel].uart_instance->p_ctrl), (tx_buff + send_i), 1);
          }
          else {
            tx_status = TX_STOPPED;
          }
          break;
        }
        case UART_EVENT_RX_CHAR:
        case UART_EVENT_BREAK_DETECT:
        case UART_EVENT_TX_DATA_EMPTY:
        {
            break;
        }
    }

}

/* -------------------------------------------------------------------------- */
UART::UART(int ch) :
  tx_st(TX_STOPPED),
  _channel(ch)
/* -------------------------------------------------------------------------- */
{ 
  _uarts[_channel] = this;
}







UART::UART(
  sci_uart_instance_ctrl_t *uart_ctrl,
  const uart_cfg_t* uart_config,
  dtc_instance_ctrl_t* dtc_ctrl, int ch) :
        _uart_ctrl(uart_ctrl),
        _uart_config(uart_config),
        _dtc_ctrl(dtc_ctrl),
        _channel(ch)
{ }




/* -------------------------------------------------------------------------- */
bool UART::setUpUartIrqs(uart_cfg_t &cfg) {
/* -------------------------------------------------------------------------- */  
  bool rv = false;

  
  if(_channel == 2) {
    rv = IRQManager::getInstance().addPeripheral(UART_SCI2,cfg);
  }
  
  return rv;

} 


void UART::begin(unsigned long baudrate, uint16_t config) {
  bool isSerialObject = false;

  EPeripheralBus periphBusCfg = NOT_A_BUS;

#if SERIAL_HOWMANY > 0
  if (_channel == UART1_CHANNEL) {
    isSerialObject = true;
    periphBusCfg = SERIAL_BUS;
  }
#endif
#if SERIAL_HOWMANY > 1
  if (_channel == UART2_CHANNEL) {
    isSerialObject = true;
    periphBusCfg = SERIAL1_BUS;
  }
#endif
#if SERIAL_HOWMANY > 2
  if (_channel == UART3_CHANNEL) {
    isSerialObject = true;
    periphBusCfg = SERIAL2_BUS;
  }
#endif
#if SERIAL_HOWMANY > 3
  if (_channel == UART4_CHANNEL) {
    isSerialObject = true;
    periphBusCfg = SERIAL3_BUS;
  }
#endif
#if SERIAL_HOWMANY > 4
  if (_channel == UART5_CHANNEL) {
    isSerialObject = true;
    periphBusCfg = SERIAL4_BUS;
  }
#endif

  if (isSerialObject) {
    int pin_count = 0;
    bsp_io_port_pin_t serial_pins[4];
    for (int i=0; i<PINCOUNT_fn(); i++) {
      if (g_APinDescription[i].PeripheralConfig == periphBusCfg) {
        serial_pins[pin_count] = g_APinDescription[i].name;
        pin_count++;
      }
      if (pin_count == 2) break;
    }
    setPins(serial_pins[0], serial_pins[1]);
  }

  

  _uart_ctrl = (uart_ctrl_t*)(SciTable[_channel].uart_instance->p_ctrl);
  _uart_config = (const uart_cfg_t *)(SciTable[_channel].uart_instance->p_cfg);

  fsp_err_t err;

  _config = *_uart_config;

  _config.p_callback = UART::WrapperCallback;

  switch(config){
      case SERIAL_8N1:
          _config.data_bits = UART_DATA_BITS_8;
          _config.parity = UART_PARITY_OFF;
          _config.stop_bits = UART_STOP_BITS_1;
          break;
      case SERIAL_8N2:
          _config.data_bits = UART_DATA_BITS_8;
          _config.parity = UART_PARITY_OFF;
          _config.stop_bits = UART_STOP_BITS_2;
          break;
      case SERIAL_8E1:
          _config.data_bits = UART_DATA_BITS_8;
          _config.parity = UART_PARITY_EVEN;
          _config.stop_bits = UART_STOP_BITS_1;
          break;
      case SERIAL_8E2:
          _config.data_bits = UART_DATA_BITS_8;
          _config.parity = UART_PARITY_EVEN;
          _config.stop_bits = UART_STOP_BITS_2;
          break;
      case SERIAL_8O1:
          _config.data_bits = UART_DATA_BITS_8;
          _config.parity = UART_PARITY_ODD;
          _config.stop_bits = UART_STOP_BITS_1;
          break;
      case SERIAL_8O2:
          _config.data_bits = UART_DATA_BITS_8;
          _config.parity = UART_PARITY_ODD;
          _config.stop_bits = UART_STOP_BITS_2;
          break;
  }

  const bool bit_mod = true;
  const uint32_t err_rate = 5;

  //enableUartIrqs();
  setUpUartIrqs(_config);

  uint8_t *tx_array = new uint8_t[SERIAL_TX_BUFFER_SIZE];
  uint8_t *rx_array = new uint8_t[SERIAL_RX_BUFFER_SIZE];

  _tx_buffer[_channel] = tx_array;
  _rx_buffer[_channel] = rx_array;

  err = R_SCI_UART_BaudCalculate(baudrate, bit_mod, err_rate, &_baud);
  err = R_SCI_UART_Open (_uart_ctrl, &_config);
  if(err != FSP_SUCCESS) while(1);
  err = R_SCI_UART_BaudSet(_uart_ctrl, (void *) &_baud);
  if(err != FSP_SUCCESS) while(1);
  err = R_SCI_UART_Read(_uart_ctrl, _rx_buffer[_channel], SERIAL_RX_BUFFER_SIZE);
  if(err != FSP_SUCCESS) while(1);
  _begin = true;
}

void UART::begin(unsigned long baudrate) {
  begin(baudrate, SERIAL_8N1);
}

void UART::end() {
  R_SCI_UART_Close (_uart_ctrl);
}

void UART::setPins(int tx, int rx, int rts, int cts)
{
  setPins(digitalPinToBspPin(tx), digitalPinToBspPin(rx),
          digitalPinToBspPin(rts), digitalPinToBspPin(cts));
}

void UART::setPins(bsp_io_port_pin_t tx, bsp_io_port_pin_t rx,
                      bsp_io_port_pin_t rts, bsp_io_port_pin_t cts)
{
  pinPeripheral(tx, (uint32_t) IOPORT_CFG_PERIPHERAL_PIN | getPinConfig(tx));
  pinPeripheral(rx, (uint32_t) IOPORT_CFG_PERIPHERAL_PIN | getPinConfig(rx));
  if (rts != (bsp_io_port_pin_t)0) {
    pinPeripheral(rts, (uint32_t) IOPORT_CFG_PERIPHERAL_PIN | getPinConfig(rts));
    pinPeripheral(cts, (uint32_t) IOPORT_CFG_PERIPHERAL_PIN | getPinConfig(cts));
  }
}

int UART::available() {
  return ((unsigned int)(SERIAL_RX_BUFFER_SIZE + get_rx_buffer_head() - _rx_buffer_tail[_channel])) % SERIAL_RX_BUFFER_SIZE;
}

int UART::peek() {
  if (get_rx_buffer_head() == _rx_buffer_tail[_channel]) {
    return -1;
  } else {
    return _rx_buffer[_channel][_rx_buffer_tail[_channel]];
  }
}

int UART::read() {
  if (get_rx_buffer_head() == _rx_buffer_tail[_channel]) {
    return -1;
  } else {
    unsigned char c = _rx_buffer[_channel][_rx_buffer_tail[_channel]];
    _rx_buffer_tail[_channel] = (rx_buffer_index_t)(_rx_buffer_tail[_channel] + 1) % SERIAL_RX_BUFFER_SIZE;
    return c;
  }
}

void UART::flush() {
  while (_tx_buffer_head[_channel] != _tx_buffer_tail[_channel]){};
  while (_sending[_channel]){};
}









size_t UART::write(uint8_t c) {
  
  while(to_send_i == previous(send_i, SERIAL_TX_BUFFER_SIZE)) { }
  inc(to_send_i, SERIAL_TX_BUFFER_SIZE);
  tx_buff[to_send_i] = c;
  if(tx_status == TX_STOPPED) {
    tx_status = TX_STARTED;
    inc(send_i, SERIAL_TX_BUFFER_SIZE);
    R_SCI_UART_Write (_uart_ctrl, tx_buff + send_i, 1);
  }

  return 1;
}

UART::operator bool() {
	return true;
}

void UART::_tx_udr_empty_irq(void)
{
  if (_tx_buffer_head[_channel] != _tx_buffer_tail[_channel]) {
      
    _tx_buffer_tail[_channel] = (tx_buffer_index_t)((_tx_buffer_tail[_channel] + 1) % SERIAL_TX_BUFFER_SIZE);
  } else {
    _sending[_channel] = false;
  }
}


rx_buffer_index_t UART::get_rx_buffer_head()
{
  transfer_properties_t p_properties;
  fsp_err_t err;
  err = R_DTC_InfoGet(_dtc_ctrl, &p_properties);
  if(err != FSP_SUCCESS) while(1);
  return (rx_buffer_index_t)(SERIAL_RX_BUFFER_SIZE - p_properties.transfer_length_remaining);
}




#if SERIAL_HOWMANY > 0
UART _UART1_(UART1_CHANNEL);
#endif

#if SERIAL_HOWMANY > 1
UART _UART2_(UART2_CHANNEL);
#endif

#if SERIAL_HOWMANY > 2
UART _UART3_(UART3_CHANNEL);
#endif

#if SERIAL_HOWMANY > 3
UART _UART4_(UART4_CHANNEL);
#endif

#if SERIAL_HOWMANY > 4
UART _UART5_(UART5_CHANNEL);
#endif